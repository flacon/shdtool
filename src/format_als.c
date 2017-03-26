/*  format_als.c - als format module
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

CVSID("$Id: format_als.c,v 1.5 2009/03/11 17:18:01 jason Exp $")

#define ALS "mp4als"

#define ALS_MAGIC "ALS"

static char default_decoder_args[] = "-x " FILENAME_PLACEHOLDER " -";
static char default_encoder_args[] = "- " FILENAME_PLACEHOLDER;

format_module format_als = {
  "als",
  "MPEG-4 Audio Lossless Coding",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  "-",
  ALS_MAGIC,
  0,
  "als",
  ALS,
  default_decoder_args,
  ALS,
  default_encoder_args,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
