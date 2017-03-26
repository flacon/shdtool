/*  mode_strip.c - strip mode module
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

CVSID("$Id: mode_strip.c,v 1.105 2009/03/17 17:23:05 jason Exp $")

static bool strip_main(int,char **);
static void strip_help(void);

mode_module mode_strip = {
  "strip",
  "shnstrip",
  "Strips extra RIFF chunks and/or writes canonical headers",
  CVSIDSTR,
  TRUE,
  strip_main,
  strip_help
};

#define STRIP_POSTFIX "-stripped"

static bool strip_header = TRUE;
static bool strip_chunks = TRUE;

static void strip_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c      don't strip unnecessary RIFF chunks\n");
  st_info("  -e      don't rewrite WAVE header in canonical format\n");
  st_info("  -h      show this help screen\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = INPUT_FILE_DIR;
  st_ops.output_postfix = STRIP_POSTFIX;

  while ((c = st_getopt(argc,argv,"ce")) != -1) {
    switch (c) {
      case 'c':
        strip_chunks = FALSE;
        break;
      case 'e':
        strip_header = FALSE;
        break;
    }
  }

  if (!strip_header && !strip_chunks)
    st_help("nothing to do if not stripping headers or RIFF chunks\n");

  *first_arg = optind;
}

static bool strip_and_canonicize(wave_info *info)
{
  wint new_header_size;
  wlong new_chunk_size;
  unsigned char *header = NULL;
  char outfilename[FILENAME_SIZE];
  FILE *output = NULL;
  proc_info output_proc;
  long possible_extra_stuff;
  bool has_null_pad,success;
  progress_info proginfo;

  success = FALSE;

  create_output_filename(info->filename,info->input_format->extension,outfilename);

  proginfo.initialized = FALSE;
  proginfo.prefix = "Stripping";
  proginfo.clause = "-->";
  proginfo.filename1 = info->filename;
  proginfo.filedesc1 = info->m_ss;
  proginfo.filename2 = outfilename;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = info->total_size;

  prog_update(&proginfo);

  if (PROB_HDR_INCONSISTENT(info)) {
    prog_error(&proginfo);
    st_warning("file has an inconsistent header -- skipping.");
    return FALSE;
  }

  if (PROB_TRUNCATED(info)) {
    prog_error(&proginfo);
    st_warning("file seems to be truncated -- skipping.");
    return FALSE;
  }

  if (strip_header && strip_chunks && !PROB_EXTRA_CHUNKS(info) && !PROB_HDR_NOT_CANONICAL(info)) {
    prog_error(&proginfo);
    st_warning("file already has a canonical header and no extra RIFF chunks -- skipping.");
    return FALSE;
  }

  if (strip_chunks && !strip_header && !PROB_EXTRA_CHUNKS(info)) {
    prog_error(&proginfo);
    st_warning("file already has no extra RIFF chunks -- skipping.");
    return FALSE;
  }

  if (strip_header && !strip_chunks && !PROB_HDR_NOT_CANONICAL(info)) {
    prog_error(&proginfo);
    st_warning("file already has a canonical header -- skipping.");
    return FALSE;
  }

  if (files_are_identical(info->filename,outfilename)) {
    prog_error(&proginfo);
    st_warning("output file would overwrite input file -- skipping.");
    return FALSE;
  }

  has_null_pad = odd_sized_data_chunk_is_null_padded(info);

  if (!has_null_pad)
    info->extra_riff_size++;

  new_header_size = info->header_size;
  possible_extra_stuff = (info->extra_riff_size > 0) ? info->extra_riff_size : 0;

  if (strip_header)
    new_header_size = CANONICAL_HEADER_SIZE;

  if (strip_chunks)
    possible_extra_stuff = 0;

  new_chunk_size = info->chunk_size - (info->header_size - new_header_size) - (info->extra_riff_size - possible_extra_stuff);

  if (!open_input_stream(info)) {
    prog_error(&proginfo);
    st_warning("could not reopen input file -- skipping.");
    return FALSE;
  }

  if (NULL == (output = open_output_stream(outfilename,&output_proc))) {
    prog_error(&proginfo);
    st_warning("could not open output file -- skipping.");
    goto cleanup;
  }

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_warning("could not allocate %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while reading %d bytes of data -- skipping.",info->header_size);
    goto cleanup;
  }

  /* already read in header, now check to see if we need to overwrite that with a canonical one */
  if (strip_header)
    make_canonical_header(header,info);

  put_chunk_size(header,new_chunk_size);

  if (write_n_bytes(output,header,new_header_size,NULL) != new_header_size) {
    prog_error(&proginfo);
    st_warning("error while writing %d bytes of data -- skipping.",new_header_size);
    goto cleanup;
  }

  if (transfer_n_bytes(info->input,output,info->data_size,NULL) != info->data_size) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu bytes of data -- skipping.",info->data_size);
    goto cleanup;
  }

  if (PROB_ODD_SIZED_DATA(info) && has_null_pad && (1 != transfer_n_bytes(info->input,output,1,NULL))) {
    prog_error(&proginfo);
    st_warning("error while transferring NULL pad byte -- skipping.");
    goto cleanup;
  }

  if ((possible_extra_stuff > 0) && (transfer_n_bytes(info->input,output,possible_extra_stuff,NULL) != possible_extra_stuff)) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu extra bytes -- skipping.",possible_extra_stuff);
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

  close_input_stream(info);

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  success = strip_and_canonicize(info);

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

static bool strip_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
