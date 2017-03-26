/*  format_tak.c - tak format module
 *  Copyright (C) 2000-2009  Jason Jordan <shnutils@freeshell.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "format.h"

CVSID("$Id: format_tak.c,v 1.12 2009/03/11 17:18:01 jason Exp $")

#define TAK "takc"

#define TAK_MAGIC "tBaK"

static char default_decoder_args[] = "-d -silent " FILENAME_PLACEHOLDER " -";
static char default_encoder_args[] = "-e -silent -ihs - " FILENAME_PLACEHOLDER;

format_module format_tak = {
  "tak",
  "(T)om's lossless (A)udio (K)ompressor",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  TRUE,
  NULL,
  TAK_MAGIC,
  0,
  "tak",
  TAK,
  default_decoder_args,
  TAK,
  default_encoder_args,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
