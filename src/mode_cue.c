/*  mode_cue.c - cue sheet mode module
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

CVSID("$Id: mode_cue.c,v 1.48 2009/03/17 17:23:05 jason Exp $")

static bool cue_main(int,char **);
static void cue_help(void);

mode_module mode_cue = {
  "cue",
  "shncue",
  "Generates a CUE sheet or split points from a set of files",
  CVSIDSTR,
  FALSE,
  cue_main,
  cue_help
};

enum {
  TYPE_UNKNOWN,
  TYPE_CUESHEET,
  TYPE_SPLITPOINTS
};

static int output_type = TYPE_UNKNOWN;

static double total_data_size = 0.0;
static int numfiles = 0;

static wave_info *totals = NULL;

static void cue_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c      generate CUE sheet (default)\n");
  st_info("  -h      show this help screen\n");
  st_info("  -s      generate split points in explicit byte-offset format\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  output_type = TYPE_CUESHEET;

  while ((c = st_getopt(argc,argv,"cs")) != -1) {
    switch (c) {
      case 'c':
        output_type = TYPE_CUESHEET;
        break;
      case 's':
        output_type = TYPE_SPLITPOINTS;
        break;
    }
  }

  *first_arg = optind;
}

static void verify_wave_info(wave_info *info)
{
  if ((TYPE_CUESHEET == output_type) && PROB_NOT_CD(info)) {
    st_error("file is not CD-quality: [%s]",info->filename);
  }

  if (0 == totals->wave_format && 0 == totals->channels &&
      0 == totals->samples_per_sec && 0 == totals->avg_bytes_per_sec &&
      0 == totals->bits_per_sample && 0 == totals->block_align) {
    totals->wave_format = info->wave_format;
    totals->channels = info->channels;
    totals->samples_per_sec = info->samples_per_sec;
    totals->avg_bytes_per_sec = info->avg_bytes_per_sec;
    totals->bits_per_sample = info->bits_per_sample;
    totals->block_align = info->block_align;
    totals->problems = info->problems;
    return;
  }

  if (info->wave_format != totals->wave_format)
    st_error("WAVE format differs among these files");

  if (info->channels != totals->channels)
    st_error("number of channels differs among these files");

  if (info->samples_per_sec != totals->samples_per_sec)
    st_error("samples per second differs among these files");

  if (info->avg_bytes_per_sec != totals->avg_bytes_per_sec)
    st_error("average bytes per second differs among these files");

  if (info->bits_per_sample != totals->bits_per_sample)
    st_error("bits per sample differs among these files");

  if (info->block_align != totals->block_align) {
    st_warning("block align differs among these files");
  }
}

static void output_init()
{
  if (NULL == (totals = new_wave_info(NULL)))
    st_error("could not allocate memory for totals");

  if (output_type == TYPE_CUESHEET) {
    st_output("FILE \"joined.wav\" WAVE\n");
  }
}

static bool output_track(char *filename)
{
  wave_info *info;
  wlong curr_data_size = 0;
  char *p;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  verify_wave_info(info);

  numfiles++;

  curr_data_size = info->data_size;

  if (output_type == TYPE_CUESHEET) {
    info->data_size = (wlong)total_data_size;
    info->length = info->data_size / info->rate;
    length_to_str(info);
    if ((p = strstr(info->m_ss,".")))
      *p = ':';
    st_output("  TRACK %02d AUDIO\n",numfiles);
    st_output("    INDEX 01 %s\n",info->m_ss);
  }
  else if (output_type == TYPE_SPLITPOINTS) {
    if (total_data_size > 0.0)
      st_output("%0.0f\n",total_data_size);
  }

  total_data_size += (double)curr_data_size;

  return TRUE;
}

static void output_end()
{
  if (TYPE_CUESHEET == output_type && numfiles < 1) {
    st_error("need one or more files in order to generate CUE sheet");
  }

  if (TYPE_SPLITPOINTS == output_type && numfiles < 2) {
    st_error("need two or more files in order to generate split points");
  }
}

static bool process(int argc,char **argv,int start)
{
  char *filename;
  bool success;

  success = TRUE;

  output_init();

  input_init(start,argc,argv);

  while ((filename = input_get_filename())) {
    success = (output_track(filename) && success);
  }

  output_end();

  return success;
}

static bool cue_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
