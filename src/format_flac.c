/*  format_flac.c - flac format module
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

CVSID("$Id: format_flac.c,v 1.57 2009/03/11 17:18:01 jason Exp $")

#define FLAC "flac"

#define FLAC_MAGIC "fLaC"

static char default_decoder_args[] = "-c -d -s " FILENAME_PLACEHOLDER;
static char default_encoder_args[] = "-s -o " FILENAME_PLACEHOLDER " -";

format_module format_flac = {
  "flac",
  "Free Lossless Audio Codec",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  NULL,
  FLAC_MAGIC,
  0,
  "flac",
  FLAC,
  default_decoder_args,
  FLAC,
  default_encoder_args,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
