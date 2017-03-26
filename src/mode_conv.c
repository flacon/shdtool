/*  mode_conv.c - conv mode module
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

CVSID("$Id: mode_conv.c,v 1.111 2009/03/30 06:31:20 jason Exp $")

static bool conv_main(int,char **);
static void conv_help(void);

mode_module mode_conv = {
  "conv",
  "shnconv",
  "Converts files from one format to another",
  CVSIDSTR,
  TRUE,
  conv_main,
  conv_help
};

static bool read_from_terminal = FALSE;

static void conv_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -h      show this help screen\n");
  st_info("  -t      read WAVE data from the terminal\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = INPUT_FILE_DIR;

  while ((c = st_getopt(argc,argv,"t")) != -1) {
    switch (c) {
      case 't':
        read_from_terminal = TRUE;
        break;
    }
  }

  *first_arg = optind;
}

static bool conv_file(wave_info *info)
{
  int bytes;
  proc_info output_proc;
  FILE *output = NULL;
  char outfilename[FILENAME_SIZE];
  unsigned char *header = NULL,nullpad[BUF_SIZE];
  bool success;
  progress_info proginfo;

  create_output_filename(info->filename,info->input_format->extension,outfilename);

  success = FALSE;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Converting";
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

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_warning("could not allocate %d-byte WAVE header -- skipping.",info->header_size);
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

  if ((info->header_size > 0) && write_n_bytes(output,header,info->header_size,&proginfo) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while writing %d-byte WAVE header -- skipping.",info->header_size);
    goto cleanup;
  }

  if ((info->data_size > 0) && (transfer_n_bytes(info->input,output,info->data_size,&proginfo) != info->data_size)) {
    prog_error(&proginfo);
    st_warning("error while transferring %lu-byte data chunk -- skipping.",info->data_size);
    goto cleanup;
  }

  if (PROB_ODD_SIZED_DATA(info)) {
    nullpad[0] = 1;

    bytes = read_n_bytes(info->input,nullpad,1,&proginfo);

    if ((1 != bytes) && (0 != bytes)) {
      prog_error(&proginfo);
      st_warning("error while reading possible NULL pad byte from input file -- skipping.");
      goto cleanup;
    }

    if ((0 == bytes) || ((1 == bytes) && nullpad[0])) {
      st_debug1("missing NULL pad byte for odd-sized data chunk in file: [%s]",info->filename);
    }

    if (1 == bytes) {
      if (0 == nullpad[0]) {
        if (write_n_bytes(output,nullpad,1,&proginfo) != 1) {
          prog_error(&proginfo);
          st_warning("error while writing NULL pad byte -- skipping.");
          goto cleanup;
        }
      }
      else {
        if (write_n_bytes(output,nullpad,1,&proginfo) != 1) {
          prog_error(&proginfo);
          st_warning("error while writing initial extra byte -- skipping.");
          goto cleanup;
        }
      }
    }
  }

  if (PROB_EXTRA_CHUNKS(info) && (transfer_n_bytes(info->input,output,info->extra_riff_size,&proginfo) != info->extra_riff_size)) {
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

static bool conv_terminal()
{
  int bytes_read,bytes_written;
  proc_info output_proc;
  FILE *output;
  char outfilename[FILENAME_SIZE];
  unsigned char buf[XFER_SIZE];
  bool success;
  progress_info proginfo;

  success = TRUE;

  create_output_filename("terminal","wav",outfilename);

  if (NULL == (output = open_output_stream(outfilename,&output_proc))) {
    st_warning("could not open output file: [%s]",outfilename);
    return FALSE;
  }

  SETBINARY_IN(stdin);

  if (isatty(fileno(stdin)))
    st_info("enter WAVE data by hand... good luck:\n");

  proginfo.initialized = FALSE;
  proginfo.prefix = "Converting";
  proginfo.clause = "-->";
  proginfo.filename1 = "data from the terminal";
  proginfo.filedesc1 = NULL;
  proginfo.filename2 = outfilename;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = (wlong)-1;

  prog_update(&proginfo);

  while (!feof(stdin)) {
    /* read data, write to encoder */
    bytes_read = fread(buf,1,XFER_SIZE,stdin);
    bytes_written = write_n_bytes(output,buf,bytes_read,&proginfo);
    if (bytes_read != bytes_written) {
      success = FALSE;
      break;
    }
  }

  if ((CLOSE_CHILD_ERROR_OUTPUT == close_output(output,output_proc)) || !success) {
    success = FALSE;
    prog_error(&proginfo);
    remove_file(outfilename);
  }
  else {
    success = TRUE;
    prog_success(&proginfo);
  }

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  success = conv_file(info);

  st_free(info);

  return success;
}

static bool process(int argc,char **argv,int start)
{
  char *filename;
  bool success;

  success = TRUE;

  if (read_from_terminal) {
    return conv_terminal();
  }

  input_init(start,argc,argv);

  while ((filename = input_get_filename())) {
    success = (process_file(filename) && success);
  }

  return success;
}

static bool conv_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
