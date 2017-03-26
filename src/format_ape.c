/*  format_ape.c - ape format module
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

CVSID("$Id: format_ape.c,v 1.54 2009/03/11 17:18:01 jason Exp $")

#define MAC "mac"

#define MAC_MAGIC "MAC "

static char default_decoder_args[] = FILENAME_PLACEHOLDER " - -d";
static char default_encoder_args[] = "- " FILENAME_PLACEHOLDER " -c2000";

static bool input_header_kluge(unsigned char *,wave_info *);

format_module format_ape = {
  "ape",
  "Monkey's Audio Compressor",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  NULL,
  MAC_MAGIC,
  0,
  "ape",
  MAC,
  default_decoder_args,
  MAC,
  default_encoder_args,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  input_header_kluge
};

static bool input_header_kluge(unsigned char *header,wave_info *info)
{
  wlong adjusted_data_size;

  adjusted_data_size = info->data_size;
  if (PROB_ODD_SIZED_DATA(info))
    adjusted_data_size++;

  if (info->chunk_size < adjusted_data_size)
    info->chunk_size = info->header_size + adjusted_data_size - 8;

  put_data_size(header,info->header_size,info->data_size);

  return TRUE;
}
