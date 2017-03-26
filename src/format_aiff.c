/*  format_aiff.c - aiff format module
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

CVSID("$Id: format_aiff.c,v 1.80 2009/03/11 17:18:01 jason Exp $")

#define SOX "sox"

static char default_decoder_args[] = "-t aiff " FILENAME_PLACEHOLDER " -t wav -";
static char default_encoder_args[] = "-t wav - -t aiff " FILENAME_PLACEHOLDER;

static bool is_our_file(char *);
static bool input_header_kluge(unsigned char *,wave_info *);

format_module format_aiff = {
  "aiff",
  "Audio Interchange File Format",
  CVSIDSTR,
  TRUE,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  "-",
  NULL,
  0,
  "aiff",
  SOX,
  default_decoder_args,
  SOX,
  default_encoder_args,
  is_our_file,
  NULL,
  NULL,
  NULL,
  NULL,
  input_header_kluge
};

static bool parse_aiff_header(char *filename,unsigned long *samples,unsigned short *channels,unsigned short *bits_per_sample)
/* generic function to parse an AIFF header and store certain values contained therein */
{
  FILE *f;
  unsigned long be_long = 0;
  unsigned short be_short = 0;
  unsigned char tag[4];
  bool is_compressed = FALSE;

  *samples = 0;
  *channels = 0;
  *bits_per_sample = 0;

  if (NULL == (f = open_input(filename)))
    return FALSE;

  /* look for FORM header */
  if (!read_tag(f,tag) || tagcmp(tag,(unsigned char *)AIFF_FORM))
    goto invalid_aiff_header;

  /* skip FORM chunk size, read in FORM type */
  if (!read_be_long(f,&be_long) || !read_tag(f,tag))
    goto invalid_aiff_header;

  /* if FORM type is not AIFF or AIFC, bail out */
  if (tagcmp(tag,(unsigned char *)AIFF_FORM_TYPE_AIFF) && tagcmp(tag,(unsigned char *)AIFF_FORM_TYPE_AIFC))
    goto invalid_aiff_header;

  if (!tagcmp(tag,(unsigned char *)AIFF_FORM_TYPE_AIFC))
    is_compressed = TRUE;

  /* now let's check AIFC compression type - it's in the COMM chunk */
  while (1) {
    /* read chunk id */
    if (!read_tag(f,tag))
      goto invalid_aiff_header;

    if (!tagcmp(tag,(unsigned char *)AIFF_COMM))
      break;

    /* not COMM, so read size of this chunk and skip it */
    if (!read_be_long(f,&be_long) || fseek(f,(long)be_long,SEEK_CUR))
      goto invalid_aiff_header;
  }

  /* now read channels, samples, and bits/sample from COMM chunk */
  if (!read_be_long(f,&be_long) || !read_be_short(f,channels) ||
      !read_be_long(f,samples) || !read_be_short(f,bits_per_sample) ||
      !read_be_long(f,&be_long) || !read_be_long(f,&be_long) ||
      !read_be_short(f,&be_short))
  {
    goto invalid_aiff_header;
  }

  if (is_compressed) {
    if (!read_tag(f,tag))
      goto invalid_aiff_header;

    if (tagcmp(tag,(unsigned char *)AIFF_COMPRESSION_NONE) && tagcmp(tag,(unsigned char *)AIFF_COMPRESSION_SOWT)) {
      st_debug1("found unsupported AIFF-C compression type [%c%c%c%c]",tag[0],tag[1],tag[2],tag[3]);
      goto invalid_aiff_header;
    }
  }

  fclose(f);
  return TRUE;

invalid_aiff_header:

  fclose(f);
  return FALSE;
}

static bool is_our_file(char *filename)
{
  unsigned long be_long=0;
  unsigned short be_short=0;

  return parse_aiff_header(filename,&be_long,&be_short,&be_short);
}

static bool input_header_kluge(unsigned char *header,wave_info *info)
/* kluge to set data_size and chunk_size, since sox can't fseek() on
 * stdout to backfill these values.  This relies on values in the
 * COMM chunk.
 */
{
  unsigned long samples = 0;
  unsigned short channels = 0,bits_per_sample = 0;

  if (!parse_aiff_header(info->filename,&samples,&channels,&bits_per_sample))
    return FALSE;

  /* set proper data size */
  info->data_size = channels * samples * (bits_per_sample/8);

  st_debug1("adjusting data size to: %lu",info->data_size);

  /* now set chunk size based on data size, and the canonical WAVE header size, which sox generates */
  info->chunk_size = info->data_size + CANONICAL_HEADER_SIZE - 8;
  if (PROB_ODD_SIZED_DATA(info))
    info->chunk_size++;

  make_canonical_header(header,info);

  return TRUE;
}
