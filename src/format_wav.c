/*  format_wav.c - WAVE format module
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

#include <stdlib.h>
#include "format.h"

CVSID("$Id: format_wav.c,v 1.61 2009/03/11 17:18:01 jason Exp $")

static FILE *open_for_input(char *,proc_info *);
static FILE *open_for_output(char *,proc_info *);
static bool is_our_file(char *);

#define WAVPACK_MAGIC "wvpk"

format_module format_wav = {
  "wav",
  "RIFF WAVE file format",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  FALSE,
  TRUE,
  FALSE,
  NULL,
  NULL,
  0,
  "wav",
  NULL,
  NULL,
  NULL,
  NULL,
  is_our_file,
  open_for_input,
  open_for_output,
  NULL,
  NULL,
  NULL
};

static FILE *open_for_input(char *filename,proc_info *pinfo)
{
  pinfo->pid = NO_CHILD_PID;
  return open_input(filename);
}

static FILE *open_for_output(char *filename,proc_info *pinfo)
{
  if (!clobber_check(filename))
    return NULL;

  pinfo->pid = NO_CHILD_PID;
  return open_output(filename);
}

static bool is_our_file(char *filename)
{
  wave_info *info;
  unsigned char buf[4];

  if (NULL == (info = new_wave_info(NULL)))
    st_error("could not allocate memory for WAVE info in wav check");

  info->filename = filename;

  if (NULL == (info->input = open_input(filename))) {
    st_free(info);
    return FALSE;
  }

  info->input_format = &format_wav;

  if (!verify_wav_header(info)) {
    fclose(info->input);
    st_free(info);
    return FALSE;
  }

  /* WavPack header might follow RIFF header - make sure this isn't a WavPack file */
  if (4 != fread(buf,1,4,info->input)) {
    fclose(info->input);
    st_free(info);
    return TRUE;
  }

  fclose(info->input);
  st_free(info);

  if (tagcmp(buf,(unsigned char *)WAVPACK_MAGIC))
    return TRUE;

  /* it's not a WAVE */
  return FALSE;
}
