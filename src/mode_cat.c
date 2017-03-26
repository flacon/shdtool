/*  mode_cat.c - cat mode module
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

CVSID("$Id: mode_cat.c,v 1.84 2009/03/30 06:31:20 jason Exp $")

static bool cat_main(int,char **);
static void cat_help(void);

mode_module mode_cat = {
  "cat",
  "shncat",
  "Writes PCM WAVE data from one or more files to the terminal",
  CVSIDSTR,
  FALSE,
  cat_main,
  cat_help
};

static bool cat_header = TRUE;
static bool cat_data = TRUE;
static bool cat_padded_data = TRUE;
static bool cat_extra = TRUE;

static void cat_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c      suppress extra RIFF chunks\n");
  st_info("  -d      suppress WAVE data\n");
  st_info("  -e      suppress WAVE headers\n");
  st_info("  -h      show this help screen\n");
  st_info("  -n      suppress NULL pad byte at end of odd-sized data chunks, if present\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  while ((c = st_getopt(argc,argv,"cden")) != -1) {
    switch (c) {
      case 'c':
        cat_extra = FALSE;
        break;
      case 'd':
        cat_data = FALSE;
        break;
      case 'e':
        cat_header = FALSE;
        break;
      case 'n':
        cat_padded_data = FALSE;
        break;
    }
  }

  if (!cat_header && !cat_data && !cat_extra)
    st_help("nothing to do if WAVE header, data and extra RIFF chunks are suppressed");

  *first_arg = optind;
}

static bool cat_file(wave_info *info)
{
  unsigned char *header,nullpad[BUF_SIZE];
  FILE *devnull,*data_dest;
  int bytes;
  progress_info proginfo;
  bool success;

  success = FALSE;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Catenating";
  proginfo.clause = NULL;
  proginfo.filename1 = info->filename;
  proginfo.filedesc1 = info->m_ss;
  proginfo.filename2 = NULL;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = info->total_size;

  prog_update(&proginfo);

  SETBINARY_OUT(stdout);

  if (NULL == (devnull = open_output(NULLDEVICE))) {
    prog_error(&proginfo);
    st_error("could not open output file: [%s]",NULLDEVICE);
  }

  if (!cat_header && !cat_data && !PROB_EXTRA_CHUNKS(info)) {
    prog_error(&proginfo);
    st_warning("input file contains no extra RIFF chunks -- nothing to do");
    return FALSE;
  }

  if (!open_input_stream(info)) {
    prog_error(&proginfo);
    st_error("could not reopen input file");
  }

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_error("could not allocate %d bytes for WAVE header",info->header_size);
  }

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size) {
    prog_error(&proginfo);
    st_error("error while reading %d-byte WAVE header",info->header_size);
  }

  if (cat_header) {
    if (!do_header_kluges(header,info)) {
      prog_error(&proginfo);
      st_error("could not fix WAVE header");
    }

    if (write_n_bytes(stdout,header,info->header_size,&proginfo) != info->header_size) {
      prog_error(&proginfo);
      st_error("error while writing %d-byte WAVE header",info->header_size);
    }
  }
  else {
    proginfo.bytes_written += info->header_size;
    prog_update(&proginfo);
  }

  if (cat_data)
    data_dest = stdout;
  else
    data_dest = devnull;

  if (transfer_n_bytes(info->input,data_dest,info->data_size,&proginfo) != info->data_size) {
    prog_error(&proginfo);
    st_error("error while transferring %lu bytes of data",info->data_size);
  }

  if (PROB_ODD_SIZED_DATA(info)) {
    nullpad[0] = 1;

    bytes = read_n_bytes(info->input,nullpad,1,NULL);

    if ((1 != bytes) && (0 != bytes)) {
      prog_error(&proginfo);
      st_error("error while reading possible NULL pad byte");
    }

    if ((0 == bytes) || ((1 == bytes) && nullpad[0])) {
      st_debug1("input file does not contain NULL pad byte for odd-sized data chunk per RIFF specs");
    }

    if (1 == bytes) {
      if ((0 == nullpad[0]) && cat_padded_data) {
        if (write_n_bytes(data_dest,nullpad,1,&proginfo) != 1) {
          prog_error(&proginfo);
          st_error("error while writing NULL pad byte");
        }
      }
      if (nullpad[0] && cat_extra) {
        if (write_n_bytes(stdout,nullpad,1,&proginfo) != 1) {
          prog_error(&proginfo);
          st_error("error while writing initial extra byte");
        }
      }
    }
  }

  if (cat_extra && PROB_EXTRA_CHUNKS(info)) {
    if (info->extra_riff_size != transfer_n_bytes(info->input,stdout,info->extra_riff_size,&proginfo)) {
      prog_error(&proginfo);
      st_error("error while transferring %lu extra bytes",info->extra_riff_size);
    }
  }

  success = TRUE;

  prog_success(&proginfo);

  fclose(devnull);

  st_free(header);

  close_input_stream(info);

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  success = cat_file(info);

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

static bool cat_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
