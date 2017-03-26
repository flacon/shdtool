/*  mode_trim.c - trim mode module
 *  Copyright (C) 2000-2009  Jason Jordan <shnutils@freeshell.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mode.h"

CVSID("$Id: mode_trim.c,v 1.56 2009/03/17 17:23:05 jason Exp $")

static bool trim_main(int,char **);
static void trim_help(void);

mode_module mode_trim = {
  "trim",
  "shntrim",
  "Trims PCM WAVE silence from the ends of files",
  CVSIDSTR,
  TRUE,
  trim_main,
  trim_help
};

#define TRIM_POSTFIX "-trimmed"

static bool trim_beginning = TRUE;
static bool trim_end = TRUE;

static void trim_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -b      only trim silence from the beginning of files\n");
  st_info("  -e      only trim silence from the end of files\n");
  st_info("  -h      show this help screen\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = INPUT_FILE_DIR;
  st_ops.output_postfix = TRIM_POSTFIX;

  while ((c = st_getopt(argc,argv,"be")) != -1) {
    switch (c) {
      case 'b':
        trim_beginning = TRUE;
        trim_end = FALSE;
        break;
      case 'e':
        trim_beginning = FALSE;
        trim_end = TRUE;
        break;
    }
  }

  *first_arg = optind;
}

static void do_read_cached(wave_info *info,char *buf,int req_bytes,progress_info *proginfo)
/* implements a local cache of data read from a file descriptor.
 * this is needed because scan_file() below only wants to read a few
 * bytes at a time, which is inefficient.
 */
{
  static char cache[XFER_SIZE];
  static int cur_pos;
  static int cur_max;
  static int cur_total;
  int bytes_read, req_filled;

  /* check if being initialized */

  if (NULL == info) {
    cur_pos = XFER_SIZE;
    cur_max = XFER_SIZE;
    cur_total = 0;
    return;
  }

  /* first, read from cached data */

  req_filled = 0;
  while (cur_pos < cur_max && req_filled < req_bytes) {
    buf[req_filled] = cache[cur_pos];
    cur_pos++;
    req_filled++;
    cur_total++;
  }

  if (req_filled == req_bytes)
    return;

  /* ran out of cached data - try to read more from the file descriptor */

  cur_pos = 0;
  cur_max = min(info->data_size - (wlong)cur_total,XFER_SIZE);

  if (cur_max != (bytes_read = fread(cache,1,cur_max,info->input))) {
    prog_error(proginfo);
    st_error("error while reading %d bytes into local cache from input file",cur_max);
  }

  proginfo->bytes_written += bytes_read;
  prog_update(proginfo);

  do_read_cached(info,buf+req_filled,req_bytes-req_filled,proginfo);
}

static void scan_file(wave_info *info,wlong *skip_beginning,wlong *skip_end,progress_info *proginfo)
{
  int sample_size,i;
  bool is_silence,found_noise;
  char sample[BUF_SIZE];
  wlong bytes_to_read,bytes_remaining,tmp_beginning_bytes,tmp_end_bytes;

  if (!open_input_stream(info)) {
    st_warning("could not open input file: [%s]",info->filename);
    return;
  }

  discard_header(info);

  sample_size = ((int)info->bits_per_sample * (int)info->channels) / 8;
  tmp_beginning_bytes = 0;
  tmp_end_bytes = 0;
  found_noise = FALSE;

  bytes_remaining = info->data_size;

  do_read_cached(NULL,sample,0,proginfo);

  while (bytes_remaining > 0) {
    bytes_to_read = min(bytes_remaining,sample_size);

    do_read_cached(info,sample,bytes_to_read,proginfo);

    /* compare this sample against silence (all zeroes) */
    is_silence = TRUE;
    for (i=0;i<bytes_to_read;i++) {
      if (sample[i]) {
        is_silence = FALSE;
        found_noise = TRUE;
        break;
      }
    }

    if (is_silence) {
      if (!found_noise) {
        tmp_beginning_bytes += bytes_to_read;
      }
      tmp_end_bytes += bytes_to_read;
    }
    else {
      tmp_end_bytes = 0;
    }

    bytes_remaining -= bytes_to_read;
  }

  close_input_stream(info);

  *skip_beginning = tmp_beginning_bytes;
  *skip_end = tmp_end_bytes;
}

static bool trim_file(wave_info *info)
{
  proc_info output_proc;
  FILE *output = NULL,*devnull = NULL;
  char outfilename[FILENAME_SIZE];
  unsigned char *header = NULL,nulltrim[BUF_SIZE];
  wlong skip_beginning = 0,skip_end = 0,data_bytes = 0;
  bool has_null_pad,success;
  progress_info proginfo;

  success = FALSE;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Scanning";
  proginfo.clause = NULL;
  proginfo.filename1 = info->filename;
  proginfo.filedesc1 = info->m_ss;
  proginfo.filename2 = NULL;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = info->total_size;

  prog_update(&proginfo);

  create_output_filename(info->filename,info->input_format->extension,outfilename);

  if (files_are_identical(info->filename,outfilename)) {
    prog_error(&proginfo);
    st_warning("output file would overwrite input file -- skipping.");
    return FALSE;
  }

  scan_file(info,&skip_beginning,&skip_end,&proginfo);

  if (!trim_beginning)
    skip_beginning = 0;
  if (!trim_end)
    skip_end = 0;

  data_bytes = info->data_size - (skip_beginning + skip_end);

  if ((skip_beginning == info->data_size) || (skip_end == info->data_size)) {
    prog_error(&proginfo);
    st_warning("input file contains nothing but silence -- skipping.");
    return FALSE;
  }

  if (data_bytes == info->data_size) {
    prog_error(&proginfo);
    st_warning("input file has no silence to trim from %s -- skipping.",
      (trim_beginning && trim_end) ? "either end" :
      ((trim_beginning) ? "the beginning" : "the end"));
    return FALSE;
  }

  prog_success(&proginfo);

  proginfo.prefix = "Trimming";
  proginfo.clause = "-->";
  proginfo.filename2 = outfilename;
  proginfo.bytes_total = info->total_size;

  prog_update(&proginfo);

  has_null_pad = odd_sized_data_chunk_is_null_padded(info);

  if (!open_input_stream(info)) {
    prog_error(&proginfo);
    st_warning("could not open input file -- skipping.");
    return FALSE;
  }

  if (NULL == (output = open_output_stream(outfilename,&output_proc))) {
    prog_error(&proginfo);
    st_warning("could not open output file -- skipping.");
    goto cleanup;
  }

  prog_update(&proginfo);

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_warning("could not allocate %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (read_n_bytes(info->input,header,info->header_size,&proginfo) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while discarding %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (!do_header_kluges(header,info)) {
    prog_error(&proginfo);
    st_warning("could not fix WAVE header -- skipping.");
    goto cleanup;
  }

  put_data_size(header,info->header_size,data_bytes);

  if (PROB_EXTRA_CHUNKS(info)) {
    if (!has_null_pad)
      info->extra_riff_size++;
    put_chunk_size(header,info->header_size+data_bytes+info->extra_riff_size-8);
  }
  else
    put_chunk_size(header,info->header_size+data_bytes-8);

  if ((info->header_size > 0) && write_n_bytes(output,header,info->header_size,&proginfo) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while writing %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (NULL == (devnull = open_output(NULLDEVICE))) {
    prog_error(&proginfo);
    st_warning("could not open output file: [%s]",NULLDEVICE);
    goto cleanup;
  }

  /* trim from beginning */
  if ((skip_beginning > 0) && (transfer_n_bytes(info->input,devnull,skip_beginning,&proginfo) != skip_beginning)) {
    prog_error(&proginfo);
    st_warning("error while trimming %lu bytes from beginning of file -- skipping.",skip_beginning);
    goto cleanup;
  }

  /* write middle data */
  if (transfer_n_bytes(info->input,output,data_bytes,&proginfo) != data_bytes) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu bytes -- skipping.",data_bytes);
    goto cleanup;
  }

  /* trim from end */
  if ((skip_end > 0) && (transfer_n_bytes(info->input,devnull,skip_end,&proginfo) != skip_end)) {
    prog_error(&proginfo);
    st_warning("error while trimming %lu bytes from end of file -- skipping.",skip_end);
    goto cleanup;
  }

  if (PROB_ODD_SIZED_DATA(info) && has_null_pad) {
    if (1 != read_n_bytes(info->input,nulltrim,1,NULL)) {
      prog_error(&proginfo);
      st_warning("error while discarding NULL pad byte");
      goto cleanup;
    }
  }

  /* write extra riff info */
  if ((info->extra_riff_size > 0) && (transfer_n_bytes(info->input,output,info->extra_riff_size,&proginfo) != info->extra_riff_size)) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu extra bytes -- skipping.",info->extra_riff_size);
    goto cleanup;
  }

  success = TRUE;

  prog_success(&proginfo);

cleanup:
  st_free(header);

  if ((output) && ((CLOSE_CHILD_ERROR_OUTPUT == close_output(output,output_proc)) || !success)) {
    success = FALSE;
    remove_file(outfilename);
  }

  if (devnull)
    fclose(devnull);

  close_input_stream(info);

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  success = trim_file(info);

  st_free(info);

  return success;
}

static bool process(int argc,char **argv,int start)
{  
  char *filename;
  bool success;

  success = TRUE;

  input_init(start,argc,argv);

  while ((filename = input_get_filename())) {
    success = (process_file(filename) && success);
  }

  return success;
}

static bool trim_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
