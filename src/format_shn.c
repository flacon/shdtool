/*  format_shn.c - shorten format module
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

#include "format.h"

CVSID("$Id: format_shn.c,v 1.65 2009/03/11 17:18:01 jason Exp $")

#define SHORTEN "shorten"

#define SHORTEN_MAGIC "ajkg"
#define SHORTEN_SEEKTABLE_MAGIC "SHNAMPSK"

static char default_decoder_args[] = "-x " FILENAME_PLACEHOLDER " -";
static char default_encoder_args[] = "- " FILENAME_PLACEHOLDER;

static void show_extra_info(char *);

format_module format_shn = {
  "shn",
  "Shorten low complexity waveform coder",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  NULL,
  SHORTEN_MAGIC,
  0,
  "shn",
  SHORTEN,
  default_decoder_args,
  SHORTEN,
  default_encoder_args,
  NULL,
  NULL,
  NULL,
  show_extra_info,
  NULL,
  NULL
};

static void show_extra_info(char *filename)
{
  FILE *f;
  unsigned char buf[9];

  st_output("  Seekable:                   ");

  if (NULL == (f = fopen(filename,"r"))) {
    st_output("(error: could not open file)\n");
    return;
  }

  if (fseek(f,-8,SEEK_END)) {
    st_output("(error: could not seek to end of file)\n");
    fclose(f);
    return;
  }

  if (8 != fread(buf,1,8,f)) {
    st_output("(error: could not read last 8 bytes of file)\n");
    fclose(f);
    return;
  }

  fclose(f);

  buf[8] = 0;

  if (!tagcmp(buf,(unsigned char *)SHORTEN_SEEKTABLE_MAGIC))
    st_output("yes\n");
  else
    st_output("no\n");
}
