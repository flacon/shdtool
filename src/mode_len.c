/*  mode_len.c - len mode module
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

CVSID("$Id: mode_len.c,v 1.90 2009/03/17 17:23:05 jason Exp $")

static bool len_main(int,char **);
static void len_help(void);

mode_module mode_len = {
  "len",
  "shnlen",
  "Displays length, size and properties of PCM WAVE data",
  CVSIDSTR,
  FALSE,
  len_main,
  len_help
};

#define LEN_OK             "-"
#define LEN_NOT_APPLICABLE "x"

typedef enum {
  LEVEL_UNKNOWN = -1,
  LEVEL_BYTES,
  LEVEL_KBYTES,
  LEVEL_MBYTES,
  LEVEL_GBYTES,
  LEVEL_TBYTES
} len_levels;

static int totals_unit_level = LEVEL_UNKNOWN;
static int file_unit_level = LEVEL_UNKNOWN;
static int num_processed = 0;
static bool all_cd_quality = TRUE;
static bool suppress_column_names = FALSE;
static bool suppress_totals_line = FALSE;

static double total_size = 0.0;
static double total_data_size = 0.0;
static double total_disk_size = 0.0;
static double total_length = 0.0;
static double unit_divs[5] = {1.0, 1024.0, 1048576.0, 1073741824.0, 1099511627776.0};

static char *units[5] = {"B ", "KB", "MB", "GB", "TB"};

static void len_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -U unit show total size in specified unit (*)\n");
  st_info("  -c      suppress column names\n");
  st_info("  -h      show this help screen\n");
  st_info("  -t      suppress totals line\n");
  st_info("  -u unit show file sizes in specified unit (*)\n");
  st_info("\n");
  st_info("          (*) unit is one of: {[b], kb, mb, gb, tb}\n");
  st_info("\n");
}

static int get_unit(char *unit)
{
  if (!strcmp(optarg,"b"))
    return LEVEL_BYTES;

  if (!strcmp(optarg,"kb"))
    return LEVEL_KBYTES;

  if (!strcmp(optarg,"mb"))
    return LEVEL_MBYTES;

  if (!strcmp(optarg,"gb"))
    return LEVEL_GBYTES;

  if (!strcmp(optarg,"tb"))
    return LEVEL_TBYTES;

  return LEVEL_UNKNOWN;
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  file_unit_level = LEVEL_BYTES;
  totals_unit_level = LEVEL_BYTES;

  while ((c = st_getopt(argc,argv,"U:ctu:")) != -1) {
    switch (c) {
      case 'U':
        if (NULL == optarg)
          st_error("missing total size unit");
        totals_unit_level = get_unit(optarg);
        if (LEVEL_UNKNOWN == totals_unit_level)
          st_help("unknown total size unit: [%s]",optarg);
        break;
      case 'c':
        suppress_column_names = TRUE;
        break;
      case 't':
        suppress_totals_line = TRUE;
        break;
      case 'u':
        if (NULL == optarg)
          st_error("missing file size unit");
        file_unit_level = get_unit(optarg);
        if (LEVEL_UNKNOWN == file_unit_level)
          st_help("unknown file size unit: [%s]",optarg);
        break;
    }
  }

  *first_arg = optind;
}

static void show_len_banner()
{
  if (suppress_column_names)
    return;

  st_output("    length     expanded size    cdr  WAVE problems  fmt   ratio  filename\n");
}

static void print_formatted_length(wave_info *info)
{
  int i,len;

  len = strlen(info->m_ss);

  if (PROB_NOT_CD(info)) {
    for (i=0;i<13-len;i++)
      st_output(" ");
    st_output("%s",info->m_ss);
  }
  else {
    for (i=0;i<12-len;i++)
      st_output(" ");
    st_output("%s ",info->m_ss);
  }
}

static bool show_stats(wave_info *info)
{
  wlong appended_bytes;
  bool success;

  success = FALSE;

  print_formatted_length(info);

  if (file_unit_level > 0)
    st_output("%14.2f",(double)info->total_size / unit_divs[file_unit_level]);
  else
    st_output("%14lu",info->total_size);

  st_output(" %s",units[file_unit_level]);

  /* CD-R properties */

  st_output("  ");

  if (PROB_NOT_CD(info))
    st_output("c%s%s",LEN_NOT_APPLICABLE,LEN_NOT_APPLICABLE);
  else {
    st_output("%s",LEN_OK);
    if (PROB_BAD_BOUND(info))
      st_output("b");
    else
      st_output("%s",LEN_OK);

    if (PROB_TOO_SHORT(info))
      st_output("s");
    else
      st_output("%s",LEN_OK);
  }

  /* WAVE properties */

  st_output("   ");

  if (PROB_HDR_NOT_CANONICAL(info))
    st_output("h");
  else
    st_output("%s",LEN_OK);

  if (PROB_EXTRA_CHUNKS(info))
    st_output("e");
  else
    st_output("%s",LEN_OK);

  /* problems */

  st_output("   ");

  if (info->file_has_id3v2_tag)
    st_output("3");
  else
    st_output("%s",LEN_OK);

  if (PROB_DATA_NOT_ALIGNED(info))
    st_output("a");
  else
    st_output("%s",LEN_OK);

  if (PROB_HDR_INCONSISTENT(info))
    st_output("i");
  else
    st_output("%s",LEN_OK);

  if (!info->input_format->is_compressed && !info->input_format->is_translated) {
    appended_bytes = info->actual_size - info->total_size - info->id3v2_tag_size;

    if (PROB_TRUNCATED(info))
      st_output("t");
    else
      st_output("%s",LEN_OK);

    if (PROB_JUNK_APPENDED(info) && appended_bytes > 0)
      st_output("j");
    else
      st_output("%s",LEN_OK);
  }
  else
    st_output("%s%s",LEN_NOT_APPLICABLE,LEN_NOT_APPLICABLE);

  /* input file format */
  st_output("  %5s",info->input_format->name);

  /* ratio */
  st_output("  %0.4f",(double)info->actual_size/(double)info->total_size);

  st_output("  %s\n",info->filename);

  success = TRUE;

  return success;
}

static void show_totals_line()
{
  wave_info *info;
  wlong seconds;
  double ratio;

  if (suppress_totals_line)
    return;

  if (NULL == (info = new_wave_info(NULL)))
    st_error("could not allocate memory for totals");

  if (all_cd_quality) {
    /* Note to users from the year 2376:  the m:ss.ff value on the totals line will only be
     * correct as long as the total data size is less than 689 terabytes (2^32 * 176400 bytes).
     * Hopefully, by then you'll all have 2048-bit processers to go with your petabyte keychain
     * raid arrays, and this won't be an issue.
     */

    /* calculate floor of total length in seconds */
    seconds = (wlong)(total_data_size / (double)CD_RATE);

    /* since length_to_str() only considers (data_size % CD_RATE) when the file is CD-quality,
     * we don't need to risk overflowing a 32-bit unsigned long with a double - we can cheat
     * and just store the modulus.
     */
    info->data_size = (wlong)(total_data_size - (double)(seconds * CD_RATE));

    info->length = seconds;
    info->rate = CD_RATE;
  }
  else {
    info->problems |= PROBLEM_NOT_CD_QUALITY;
    info->length = (wlong)total_length;
  }

  info->exact_length = total_length;

  length_to_str(info);

  print_formatted_length(info);

  ratio = (num_processed > 0) ? (total_disk_size / total_size) : 0.0;

  total_size /= unit_divs[totals_unit_level];

  if (totals_unit_level > 0)
    st_output("%14.2f",total_size);
  else
    st_output("%14.0f",total_size);

  st_output(" %s                           %0.4f  (%d file%s)\n",
    units[totals_unit_level],ratio,num_processed,(1 != num_processed)?"s":"");

  st_free(info);
}

static void update_totals(wave_info *info)
{
  total_size += (double)info->total_size;
  total_data_size += (double)info->data_size;
  total_length += (double)info->data_size / (double)info->avg_bytes_per_sec;

  if (PROB_NOT_CD(info))
    all_cd_quality = FALSE;

  total_disk_size += (double)info->actual_size;

  num_processed++;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    return FALSE;

  if ((success = show_stats(info)))
    update_totals(info);

  st_free(info);

  return success;
}

static bool process(int argc,char **argv,int start)
{
  char *filename;
  bool success;

  success = TRUE;

  show_len_banner();

  input_init(start,argc,argv);

  while ((filename = input_get_filename())) {
    success = (process_file(filename) && success);
  }

  show_totals_line();

  return success;
}

static bool len_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
