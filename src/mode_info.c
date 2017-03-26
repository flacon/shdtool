/*  mode_info.c - info mode module
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

CVSID("$Id: mode_info.c,v 1.78 2009/03/17 17:23:05 jason Exp $")

static bool info_main(int,char **);
static void info_help(void);

mode_module mode_info = {
  "info",
  "shninfo",
  "Displays detailed information about PCM WAVE data",
  CVSIDSTR,
  FALSE,
  info_main,
  info_help
};

static void info_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -h      show this help screen\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  while ((c = st_getopt(argc,argv,"")) != -1);

  *first_arg = optind;
}

static bool show_info(wave_info *info)
{
  wlong appended_bytes,missing_bytes;
  bool success;

  success = TRUE;

  st_output("-------------------------------------------------------------------------------\n");
  st_output("File name:                    %s\n",info->filename);
  st_output("Handled by:                   %s format module\n",info->input_format->name);
  st_output("Length:                       %s\n",info->m_ss);
  st_output("WAVE format:                  0x%04x (%s)\n",info->wave_format,format_to_str(info->wave_format));
  st_output("Channels:                     %hu\n",info->channels);
  st_output("Bits/sample:                  %hu\n",info->bits_per_sample);
  st_output("Samples/sec:                  %lu\n",info->samples_per_sec);
  st_output("Average bytes/sec:            %lu\n",info->avg_bytes_per_sec);
  st_output("Rate (calculated):            %lu\n",info->rate);
  st_output("Block align:                  %hu\n",info->block_align);
  st_output("Header size:                  %d bytes\n",info->header_size);
  st_output("Data size:                    %lu byte%s\n",info->data_size,(1 == info->data_size)?"":"s");
  st_output("Chunk size:                   %lu bytes\n",info->chunk_size);
  st_output("Total size (chunk size + 8):  %lu bytes\n",info->total_size);
  st_output("Actual file size:             %lu\n",info->actual_size);
  st_output("File is compressed:           %s\n",(info->input_format->is_compressed)?"yes":"no");
  st_output("Compression ratio:            %0.4f\n",(double)info->actual_size/(double)info->total_size);
  st_output("CD-quality properties:\n");
  st_output("  CD quality:                 %s\n",(PROB_NOT_CD(info))?"no":"yes");
  st_output("  Cut on sector boundary:     %s\n",(PROB_NOT_CD(info))?"n/a":((PROB_BAD_BOUND(info))?"no":"yes"));

  st_output("  Sector misalignment:        ");
  if (PROB_NOT_CD(info))
    st_output("n/a\n");
  else
    st_output("%lu byte%s\n",info->data_size % CD_BLOCK_SIZE,(1 == (info->data_size % CD_BLOCK_SIZE))?"":"s");

  st_output("  Long enough to be burned:   ");
  if (PROB_NOT_CD(info))
    st_output("n/a\n");
  else {
    if (PROB_TOO_SHORT(info))
      st_output("no -- needs to be at least %d bytes\n",CD_MIN_BURNABLE_SIZE);
    else
      st_output("yes\n");
  }

  st_output("WAVE properties:\n");
  st_output("  Non-canonical header:       %s\n",(PROB_HDR_NOT_CANONICAL(info))?"yes":"no");

  st_output("  Extra RIFF chunks:          ");
  if (PROB_EXTRA_CHUNKS(info)) {
    if (PROB_ODD_SIZED_DATA(info))
      st_output("yes (%ld or %ld bytes)\n",info->extra_riff_size,info->extra_riff_size + 1);
    else
      st_output("yes (%ld bytes)\n",info->extra_riff_size);
  }
  else
    st_output("no\n");

  st_output("Possible problems:\n");

  st_output("  File contains ID3v2 tag:    ");
  if (info->file_has_id3v2_tag)
    st_output("yes (%lu bytes)\n",info->id3v2_tag_size);
  else
    st_output("no\n");

  st_output("  Data chunk block-aligned:   ");
  if (PROB_DATA_NOT_ALIGNED(info))
    st_output("no\n");
  else
    st_output("yes\n");

  st_output("  Inconsistent header:        %s\n",(PROB_HDR_INCONSISTENT(info))?"yes":"no");

  st_output("  File probably truncated:    ");
  if (!info->input_format->is_compressed && !info->input_format->is_translated) {
    if (PROB_TRUNCATED(info)) {
      missing_bytes = info->total_size - (info->actual_size - info->id3v2_tag_size);
      st_output("yes (missing %lu byte%s)\n",missing_bytes,(1 == missing_bytes)?"":"s");
    }
    else
      st_output("no\n");
  }
  else
    st_output("unknown\n");

  st_output("  Junk appended to file:      ");
  if (!info->input_format->is_compressed && !info->input_format->is_translated) {
    appended_bytes = info->actual_size - info->total_size - info->id3v2_tag_size;
    if (PROB_JUNK_APPENDED(info) && appended_bytes > 0)
      st_output("yes (%lu byte%s)\n",appended_bytes,(1 == appended_bytes)?"":"s");
    else
      st_output("no\n");
  }
  else
    st_output("unknown\n");

  st_output("  Odd data size has pad byte: ");
  if (PROB_ODD_SIZED_DATA(info)) {
    if (-1 == info->extra_riff_size)
      st_output("no\n");
    else if (0 == info->extra_riff_size)
      st_output("yes\n");
    else
      st_output("unknown\n");
  }
  else {
    st_output("n/a\n");
  }

  if (info->input_format->extra_info) {
    st_output("Extra %s-specific info:\n",info->input_format->name);
    info->input_format->extra_info(info->filename);
  }

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  success = show_info(info);

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

static bool info_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
