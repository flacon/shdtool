/*  mode_gen.c - gen mode module
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

#include <string.h>
#include "mode.h"

CVSID("$Id: mode_gen.c,v 1.53 2009/03/16 04:46:03 jason Exp $")

static bool gen_main(int,char **);
static void gen_help(void);

mode_module mode_gen = {
  "gen",
  "shngen",
  "Generates CD-quality PCM WAVE data files containing silence",
  CVSIDSTR,
  TRUE,
  gen_main,
  gen_help
};

#define GEN_PREFIX "silence"

static char *gen_size = NULL;

static void gen_help()
{
  st_info("Usage: %s [OPTIONS]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -h      show this help screen\n");
  st_info("  -l len  generate files containing silence of length len (*)\n");
  st_info("\n");
  st_info("          (*) len must be in bytes, m:ss, m:ss.ff or m:ss.nnn format\n");
  st_info("\n");
}

static void parse(int argc,char **argv)
{
  int c;

  st_ops.output_directory = CURRENT_DIR;
  st_ops.output_prefix = GEN_PREFIX;

  while ((c = st_getopt(argc,argv,"l:")) != -1) {
    switch (c) {
      case 'l':
        if (NULL == optarg)
          st_error("missing silence length");
        gen_size = optarg;
        break;
    }
  }

  if (NULL == gen_size)
    st_error("length must be specified");
}

static bool process()
{
  wave_info *info;
  unsigned char header[CANONICAL_HEADER_SIZE],silence[XFER_SIZE];
  char outfilename[FILENAME_SIZE];
  FILE *output;
  proc_info output_proc;
  wlong gen_bytes,bytes_left,bytes;
  progress_info proginfo;
  bool success;

  success = FALSE;

  if (NULL == (info = new_wave_info(NULL))) {
    st_error("could not allocate memory for WAVE info");
  }

  info->rate = CD_RATE;

  gen_bytes = smrt_parse((unsigned char *)gen_size,info);

  create_output_filename("","",outfilename);

  info->channels = CD_CHANNELS;
  info->samples_per_sec = CD_SAMPLES_PER_SEC;
  info->avg_bytes_per_sec = CD_RATE;
  info->block_align = CD_BLOCK_ALIGN;
  info->bits_per_sample = CD_BITS_PER_SAMPLE;
  info->wave_format = WAVE_FORMAT_PCM;
  info->rate = CD_RATE;

  info->data_size = gen_bytes;
  info->chunk_size = info->data_size + CANONICAL_HEADER_SIZE - 8;
  info->length = info->data_size / (wlong)info->rate;
  info->exact_length = (double)info->data_size / (double)info->rate;

  if (PROB_ODD_SIZED_DATA(info))
    info->chunk_size++;

  make_canonical_header(header,info);

  length_to_str(info);

  bytes_left = gen_bytes;

  memset((void *)silence,0,XFER_SIZE);

  proginfo.initialized = FALSE;
  proginfo.prefix = "Generating";
  proginfo.clause = NULL;
  proginfo.filename1 = NULL;
  proginfo.filedesc1 = NULL;
  proginfo.filename2 = outfilename;
  proginfo.filedesc2 = info->m_ss;
  proginfo.bytes_total = bytes_left + CANONICAL_HEADER_SIZE;

  prog_update(&proginfo);

  if (NULL == (output = open_output_stream(outfilename,&output_proc)))
    st_error("could not open output file: [%s]",outfilename);

  if (write_n_bytes(output,header,CANONICAL_HEADER_SIZE,&proginfo) != CANONICAL_HEADER_SIZE) {
    prog_error(&proginfo);
    st_error("error while writing %d-byte WAVE header",CANONICAL_HEADER_SIZE);
  }

  while (bytes_left > 0) {
    bytes = min(bytes_left,XFER_SIZE);

    if (write_n_bytes(output,silence,bytes,&proginfo) != bytes) {
      prog_error(&proginfo);
      st_error("error while writing %d-byte chunk of silence",bytes);
    }

    bytes_left -= bytes;
  }

  if (PROB_ODD_SIZED_DATA(info) && (1 != write_padding(output,1,&proginfo))) {
    prog_error(&proginfo);
    st_error("error while NULL-padding odd-sized data chunk");
  }

  success = TRUE;

  prog_success(&proginfo);

  if (CLOSE_CHILD_ERROR_OUTPUT == close_output(output,output_proc)) {
    success = FALSE;
  }

  return success;
}

static bool gen_main(int argc,char **argv)
{
  parse(argc,argv);

  return process();
}
