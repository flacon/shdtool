/*  format_wv.c - wavpack format module
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
#include <string.h>
#include <ctype.h>
#include "format.h"

CVSID("$Id: format_wv.c,v 1.72 2009/03/11 17:18:01 jason Exp $")

#define WAVPACK  "wavpack"
#define WVUNPACK "wvunpack"

#define WAVPACK_MAGIC "wvpk"
#define WV_COMMON_HEADER_SIZE 10

#ifdef WIN32
static char default_decoder_args[] = "-q -y " FILENAME_PLACEHOLDER " -";
static char default_encoder_args[] = "-q -y - " FILENAME_PLACEHOLDER;
#else
static char default_decoder_args[] = "-q -y " FILENAME_PLACEHOLDER " -o -";
static char default_encoder_args[] = "-q -y - -o " FILENAME_PLACEHOLDER;
#endif

/* definitions for version 3 and older */
#define NEW_HIGH_FLAG        0x400  /* new high quality mode (lossless only) */
#define WavpackHeader3Format "4LSSSSLLL4L"

typedef struct _WavpackHeader3 {
  char ckID[4];
  int ckSize;
  short version;
  short bits;
  short flags, shift;
  int total_samples, crc, crc2;
  char extension[4], extra_bc, extras[3];
} WavpackHeader3;

/* definitions for version 4 */
#define HYBRID_FLAG          8
#define ID_WVC_BITSTREAM  0xb  /* these metadata identify .wvc */
#define ID_SHAPING_WEIGHTS  0x7
#define WavpackHeader4Format "4LS2LLLLL"

typedef struct _WavpackHeader4 {
  char ckID[4];
  int ckSize;
  short version;
  unsigned char track_no, index_no;
  int total_samples, block_index, block_samples, flags, crc;
} WavpackHeader4;

static bool is_our_file(char *);

format_module format_wv = {
  "wv",
  "WavPack Hybrid Lossless Audio Compression",
  CVSIDSTR,
  TRUE,
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  FALSE,
  NULL,
  NULL,
  0,
  "wv",
  WVUNPACK,
  default_decoder_args,
  WAVPACK,
  default_encoder_args,
  is_our_file,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

static char *filespec_ext(char *filespec)
{
  char *cp = filespec + strlen(filespec);

  while (--cp >= filespec) {
    if (*cp == '\\' || *cp == ':')
      return NULL;

    if (*cp == '.') {
      if (strlen (cp) > 1 && strlen (cp) <= 4)
        return cp;
      else
        return NULL;
    }
  }

  return NULL;
}

static void little_endian_to_native (void *data, char *format)
{
  unsigned char *cp = (unsigned char *) data;
  int temp;

  while (*format) {
    switch (*format) {
      case 'L':
        temp = cp [0] + (cp [1] << 8) + ((int) cp [2] << 16) + ((int) cp [3] << 24);
        * (int *) cp = temp;
        cp += 4;
        break;

      case 'S':
        temp = cp [0] + (cp [1] << 8);
        * (short *) cp = temp;
        cp += 2;
        break;

      default:
        if (isdigit (*format))
          cp += *format - '0';
        break;
    }

    format++;
  }
}

static bool file_exists_with_alternate_extension(char *filename,char *ext)
{
  char wvc_filename[FILENAME_SIZE];
  char *extp;
  FILE *f;

  strcpy(wvc_filename,filename);

  extp = filespec_ext(wvc_filename);

  if (extp)
    *extp = 0;

  strcat(wvc_filename,ext);

  if ((f = fopen(wvc_filename,"rb"))) {
    fclose(f);
    return TRUE;
  }

  return FALSE;
}

static long get_header_offset(FILE *f)
{
  unsigned char buf[4];
  long curpos;

  buf[0] = 'x';
  buf[1] = 'x';
  buf[2] = 'x';
  buf[3] = 'x';

  /* like WavPack, we check the first 1 meg of the file for a header. */
  /* unlike WavPack, we do it in the most inefficient way possible.   */

  for (curpos=0;curpos<1024*1024;curpos++) {
    buf[0] = buf[1];
    buf[1] = buf[2];
    buf[2] = buf[3];
    buf[3] = fgetc(f);

    if (feof(f)) {
      return -1;
    }

    if (!tagcmp(buf,(unsigned char *)WAVPACK_MAGIC)) {
      return curpos - 3;
    }
  }

  return -1;
}

static bool is_our_file(char *filename)
{
  wave_info *info;
  unsigned char wph[64];
  WavpackHeader3 *wph3;
  WavpackHeader4 *wph4;
  char first_id;
  int remaining_bytes;
  long header_offset;

  if (NULL == (info = new_wave_info(NULL)))
    st_error("could not allocate memory for WAVE info in wv check");

  info->filename = filename;

  if (NULL == (info->input = open_input(filename))) {
    st_free(info);
    return FALSE;
  }

  if (-1 == (header_offset = get_header_offset(info->input))) {
    fclose(info->input);
    st_free(info);
    return FALSE;
  }

  fseek(info->input,header_offset,SEEK_SET);

  /* read up to size of largest header, making sure we read enough to fill the smallest header */
  memset((void *)wph,0,64);

  if (fread(&wph,1,WV_COMMON_HEADER_SIZE,info->input) != WV_COMMON_HEADER_SIZE) {
    fclose(info->input);
    st_free(info);
    return FALSE;
  }

  if (wph[9] >= 4) {
    /* we're dealing with a version 4+ file */

    remaining_bytes = sizeof(WavpackHeader4) - WV_COMMON_HEADER_SIZE;
    if (fread(wph+WV_COMMON_HEADER_SIZE,1,remaining_bytes,info->input) != remaining_bytes) {
      fclose(info->input);
      st_free(info);
      return FALSE;
    }

    first_id = getc(info->input) & 0x1f;

    fclose(info->input);
    st_free(info);

    wph4 = (WavpackHeader4 *)wph;

    little_endian_to_native(wph4,WavpackHeader4Format);

    if (tagcmp((unsigned char *)wph4->ckID,(unsigned char *)WAVPACK_MAGIC) || wph4->version < 4 || wph4->version > 0x40f) {
      return FALSE;
    }

    st_debug1("examining version >=4 (%d) Wavpack file: [%s]",wph4->version,filename);

    if (wph4->block_samples) {
      if ((ID_WVC_BITSTREAM == first_id) || (ID_SHAPING_WEIGHTS == first_id)) {
        /* these ids belong to the correction file */
        st_debug1("this appears to be a hybrid correction file -- skipping.");
        return FALSE;
      }

      if (wph4->flags & HYBRID_FLAG) {
        /* hybrid */
        if (file_exists_with_alternate_extension(filename,".wvc") || file_exists_with_alternate_extension(filename,".WVC"))
          return TRUE;

        /* lossy */
        st_warning("encountered lossy WavPack file: [%s]",filename);
        return TRUE;
      }
    }

    /* lossless */
    return TRUE;
  }

  /* we're dealing with an older file */

  remaining_bytes = sizeof(WavpackHeader3) - WV_COMMON_HEADER_SIZE;
  if (fread(wph+WV_COMMON_HEADER_SIZE,1,remaining_bytes,info->input) != remaining_bytes) {
    fclose(info->input);
    st_free(info);
    return FALSE;
  }

  fclose(info->input);
  st_free(info);

  wph3 = (WavpackHeader3 *)wph;

  little_endian_to_native(wph3,WavpackHeader3Format);

  if (tagcmp((unsigned char *)wph3->ckID,(unsigned char *)WAVPACK_MAGIC) || wph3->version < 1 || wph3->version > 3) {
    return FALSE;
  }

  st_debug1("examining version <=3 (%d) Wavpack file: [%s]",wph3->version,filename);

  /* lossy */
  if (wph3->version == 3 && wph3->bits && (wph3->flags & NEW_HIGH_FLAG) && wph3->crc != wph3->crc2) {
    st_warning("encountered lossy WavPack file: [%s]",filename);
    return TRUE;
  }

  if (wph3->bits) {
    /* hybrid */
    if (file_exists_with_alternate_extension(filename,".wvc") || file_exists_with_alternate_extension(filename,".WVC"))
      return TRUE;

    /* lossy */
    st_warning("encountered lossy WavPack file: [%s]",filename);
    return TRUE;
  }

  /* lossless */
  return TRUE;
}
