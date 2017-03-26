/*  mode_pad.c - pad mode module
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

CVSID("$Id: mode_pad.c,v 1.81 2009/03/17 17:23:05 jason Exp $")

static bool pad_main(int,char **);
static void pad_help(void);

mode_module mode_pad = {
  "pad",
  "shnpad",
  "Pads CD-quality files not aligned on sector boundaries with silence",
  CVSIDSTR,
  TRUE,
  pad_main,
  pad_help
};

enum {
  PAD_UNKNOWN,
  PAD_PREPAD,
  PAD_POSTPAD
};

#define PAD_POSTFIX "-padded"

static int pad_type = PAD_UNKNOWN;

static void pad_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -b      pad the beginning of files with silence\n");
  st_info("  -e      pad the end of files with silence (default)\n");
  st_info("  -h      show this help screen\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = INPUT_FILE_DIR;
  st_ops.output_postfix = PAD_POSTFIX;
  pad_type = PAD_POSTPAD;

  while ((c = st_getopt(argc,argv,"be")) != -1) {
    switch (c) {
      case 'b':
        pad_type = PAD_PREPAD;
        break;
      case 'e':
        pad_type = PAD_POSTPAD;
        break;
    }
  }

  *first_arg = optind;
}

static bool pad_file(wave_info *info)
{
  int pad_bytes;
  proc_info output_proc;
  FILE *output = NULL;
  char outfilename[FILENAME_SIZE];
  unsigned char *header = NULL,nullpad[BUF_SIZE];
  bool has_null_pad;
  bool success;
  progress_info proginfo;

  success = FALSE;

  create_output_filename(info->filename,info->input_format->extension,outfilename);

  proginfo.initialized = FALSE;
  proginfo.prefix = (pad_type == PAD_PREPAD) ? "Pre-padding" : "Post-padding";
  proginfo.clause = "-->";
  proginfo.filename1 = info->filename;
  proginfo.filedesc1 = info->m_ss;
  proginfo.filename2 = outfilename;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = info->total_size;

  prog_update(&proginfo);

  if (files_are_identical(info->filename,outfilename)) {
    prog_error(&proginfo);
    st_warning("output file would overwrite input file -- skipping.");
    return FALSE;
  }

  pad_bytes = CD_BLOCK_SIZE - (info->data_size % CD_BLOCK_SIZE);

  has_null_pad = odd_sized_data_chunk_is_null_padded(info);

  if (!open_input_stream(info)) {
    prog_error(&proginfo);
    st_warning("could not open input file -- skipping.");
    return FALSE;
  }

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_warning("could not allocate %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (NULL == (output = open_output_stream(outfilename,&output_proc))) {
    prog_error(&proginfo);
    st_warning("could not open output file -- skipping.");
    goto cleanup;
  }

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while discarding %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (!do_header_kluges(header,info)) {
    prog_error(&proginfo);
    st_warning("could not fix WAVE header -- skipping.");
    goto cleanup;
  }

  put_data_size(header,info->header_size,info->data_size+pad_bytes);

  if (PROB_EXTRA_CHUNKS(info)) {
    if (!has_null_pad)
      info->extra_riff_size++;
    put_chunk_size(header,info->header_size+info->data_size+pad_bytes+info->extra_riff_size-8);
  }
  else
    put_chunk_size(header,info->header_size+info->data_size+pad_bytes-8);

  if ((info->header_size > 0) && write_n_bytes(output,header,info->header_size,&proginfo) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while writing %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if (PAD_PREPAD == pad_type) {
    if (pad_bytes != write_padding(output,pad_bytes,&proginfo)) {
      prog_error(&proginfo);
      st_warning("error while pre-padding with %d zero-bytes -- skipping.",pad_bytes);
      goto cleanup;
    }
  }

  if ((info->data_size > 0) && (transfer_n_bytes(info->input,output,info->data_size,&proginfo) != info->data_size)) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu-byte data chunk -- skipping.",info->data_size);
    goto cleanup;
  }

  if (PAD_POSTPAD == pad_type) {
    if (pad_bytes != write_padding(output,pad_bytes,&proginfo)) {
      prog_error(&proginfo);
      st_warning("error while post-padding with %d zero-bytes -- skipping",pad_bytes);
      goto cleanup;
    }
  }

  if (PROB_ODD_SIZED_DATA(info) && has_null_pad) {
    if (1 != read_n_bytes(info->input,nullpad,1,&proginfo)) {
      prog_error(&proginfo);
      st_warning("error while discarding NULL pad byte -- skipping.");
      goto cleanup;
    }
  }

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

  close_input_stream(info);

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  if (PROB_NOT_CD(info)) {
    st_warning("file is not CD-quality: [%s]",filename);
    st_free(info);
    return FALSE;
  }

  if (!PROB_BAD_BOUND(info)) {
    st_warning("file is already sector-aligned: [%s]",filename);
    st_free(info);
    return FALSE;
  }

  success = pad_file(info);

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

static bool pad_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
