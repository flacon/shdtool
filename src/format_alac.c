/*  format_alac.c - alac format module
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

CVSID("$Id: format_alac.c,v 1.39 2009/03/11 17:18:01 jason Exp $")

#define ALAC "alac"

#define ALAC_MAGIC "M4A "

static char default_decoder_args[] = FILENAME_PLACEHOLDER;

/*
 * in theory, the ffmpeg commands below should work, but currently it seems that
 * incorrect data sizes are written when encoding or decoding over pipes...
 */

/*
#define FFMPEG "ffmpeg"

static char default_decoder_args[] = "-i " FILENAME_PLACEHOLDER " -f wav -";
static char default_encoder_args[] = "-i - -acodec alac " FILENAME_PLACEHOLDER;
*/

format_module format_alac = {
  "alac",
  "Apple Lossless Audio Codec",
  CVSIDSTR,
  TRUE,
  FALSE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  "-",
  ALAC_MAGIC,
  8,
  "m4a",
  ALAC,
  default_decoder_args,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
