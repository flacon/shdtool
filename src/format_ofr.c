/*  format_ofr.c - ofr format module
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

CVSID("$Id: format_ofr.c,v 1.56 2009/03/11 17:18:01 jason Exp $")

#define OPTIMFROG "ofr"

#define OPTIMFROG_MAGIC     "OFR "
#define OPTIMFROG_MAGIC_OLD "*RIFF"

static char default_decoder_args[] = "--decode " FILENAME_PLACEHOLDER " --output -";
static char default_encoder_args[] = "--encode - --output " FILENAME_PLACEHOLDER;

static bool is_our_file(char *);

format_module format_ofr = {
  "ofr",
  "OptimFROG Lossless WAVE Audio Coder",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  "-",
  NULL,
  0,
  "ofr",
  OPTIMFROG,
  default_decoder_args,
  OPTIMFROG,
  default_encoder_args,
  is_our_file,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

static bool is_our_file(char *filename)
{
  return (check_for_magic(filename,OPTIMFROG_MAGIC,0) || check_for_magic(filename,OPTIMFROG_MAGIC_OLD,0));
}
