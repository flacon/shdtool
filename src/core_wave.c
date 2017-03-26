/*  core_wave.c - functions for parsing WAVE headers
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

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include "shntool.h"

CVSID("$Id: core_wave.c,v 1.112 2009/03/11 17:18:01 jason Exp $")

bool is_valid_file(wave_info *info)
/* determines whether the given filename (info->filename) is a regular file, and is readable */
{
  struct stat sz;
  FILE *f;

  if (stat(info->filename,&sz)) {
    if (errno == ENOENT)
      st_warning("cannot open non-existent file: [%s]",info->filename);
    else if (errno == EACCES)
      st_warning("insufficient permissions to open file: [%s]",info->filename);
    else if (errno == EFAULT)
      st_warning("encountered bad address while opening file: [%s]",info->filename);
    else if (errno == ENOMEM)
      st_warning("kernel ran out of memory while opening file: [%s]",info->filename);
    else if (errno == ENAMETOOLONG)
      st_warning("filename too long to open file: [%s]",info->filename);
    else
      st_warning("encountered system error [%s] while opening file: [%s]",strerror(errno),info->filename);
    return FALSE;
  }
  if (!S_ISREG(sz.st_mode)) {
    if (S_ISDIR(sz.st_mode))
      st_warning("cannot open directory: [%s]",info->filename);
    else if (S_ISCHR(sz.st_mode))
      st_warning("cannot open character device: [%s]",info->filename);
    else if (S_ISBLK(sz.st_mode))
      st_warning("cannot open block device: [%s]",info->filename);
    else if (S_ISFIFO(sz.st_mode))
      st_warning("cannot open named pipe: [%s]",info->filename);
#ifndef WIN32
#ifdef S_ISSOCK
    else if (S_ISSOCK(sz.st_mode))
      st_warning("cannot open socket: [%s]",info->filename);
#endif
    else if (S_ISLNK(sz.st_mode))
      st_warning("cannot open symbolic link: [%s]",info->filename);
#endif
    return FALSE;
  }

  info->actual_size = (wlong)sz.st_size;

  if (NULL == (f = fopen(info->filename,"rb"))) {
    st_warning("encountered error [%s] while opening file: [%s]",strerror(errno),info->filename);
    return FALSE;
  }

  fclose(f);

  return TRUE;
}

bool do_header_kluges(unsigned char *header,wave_info *info)
{
  bool retval = TRUE;

  if (info->input_format && info->input_format->input_header_kluge) {
    st_debug1("performing [%s] input header kluge on file: [%s]",info->input_format->name,info->filename);
    retval = info->input_format->input_header_kluge(header,info);
  }

  if (!retval)
    st_warning("[%s] input header kluge failed while processing file: [%s]",info->input_format->name,info->filename);

  return retval;
}

bool verify_wav_header_internal(wave_info *info,bool verbose)
/* verifies that data coming in on the file descriptor info->input describes a valid WAVE header */
{
  unsigned long le_long=0,bytes;
  unsigned char buf[2];
  unsigned char tag[4];
  int header_len = 0;

  /* look for "RIFF" in header */
  if (!read_tag(info->input,tag) || tagcmp(tag,(unsigned char *)WAVE_RIFF)) {
    if (verbose) {
      if (!tagcmp(tag,(unsigned char *)AIFF_FORM)) {
        st_warning("encountered unsupported AIFF data while processing file: [%s]",info->filename);
      }
      else {
        st_warning("WAVE header is missing RIFF tag while processing file: [%s]",info->filename);
      }
    }

    return FALSE;
  }

  if (!read_le_long(info->input,&info->chunk_size)) {
    st_warning("could not read chunk size from WAVE header while processing file: [%s]",info->filename);
    return FALSE;
  }

  /* look for "WAVE" in header */
  if (!read_tag(info->input,tag) || tagcmp(tag,(unsigned char *)WAVE_WAVE)) {
    st_warning("WAVE header is missing WAVE tag while processing file: [%s]",info->filename);
    return FALSE;
  }

  header_len += 12;

  st_debug1("showing RIFF chunks in file: [%s]",info->filename);

  for (;;) {
    if (!read_tag(info->input,tag) || !read_le_long(info->input,&le_long)) {
      st_warning("reached end of file while looking for fmt tag while processing file: [%s]",info->filename);
      return FALSE;
    }

    st_debug1("found chunk: [%c%c%c%c] with length: %lu",tag[0],tag[1],tag[2],tag[3],le_long);

    header_len += 8;

    if (!tagcmp(tag,(unsigned char *)WAVE_FMT))
      break;

    bytes = le_long;

    while (bytes > 0) {
      if (fread(buf,1,1,info->input) != 1) {
        st_warning("reached end of file when jumping ahead %lu bytes during search for fmt tag while processing file: [%s]",info->filename,le_long);
        return FALSE;
      }
      bytes--;
    }

    header_len += le_long;
  }

  if (le_long < 16) {
    st_warning("fmt chunk in WAVE header was too short while processing file: [%s]",info->filename);
    return FALSE;
  }

  /* now we read the juicy stuff */
  if (!read_le_short(info->input,&info->wave_format)) {
    st_warning("reached end of file while reading format while processing file: [%s]",info->filename);
    return FALSE;
  }

  switch (info->wave_format) {
    case WAVE_FORMAT_PCM:
      break;
    default:
      st_warning("unsupported format 0x%04x (%s) while processing file: [%s]",
            info->wave_format,format_to_str(info->wave_format),info->filename);
      return FALSE;
  }

  if (!read_le_short(info->input,&info->channels)) {
    st_warning("reached end of file reading channels while processing file: [%s]",info->filename);
    return FALSE;
  }

  if (!read_le_long(info->input,&info->samples_per_sec)) {
    st_warning("reached end of file reading samples/sec while processing file: [%s]",info->filename);
    return FALSE;
  }

  if (!read_le_long(info->input,&info->avg_bytes_per_sec)) {
    st_warning("reached end of file reading average bytes/sec while processing file: [%s]",info->filename);
    return FALSE;
  }

  if (!read_le_short(info->input,&info->block_align)) {
    st_warning("reached end of file reading block align while processing file: [%s]",info->filename);
    return FALSE;
  }

  if (!read_le_short(info->input,&info->bits_per_sample)) {
    st_warning("reached end of file reading bits/sample while processing file: [%s]",info->filename);
    return FALSE;
  }

  header_len += 16;

  le_long -= 16;

  if (le_long) {
    bytes = le_long;
    while (bytes > 0) {
      if (fread(buf,1,1,info->input) != 1) {
        st_warning("reached end of file jumping ahead %lu bytes while processing file: [%s]",le_long,info->filename);
        return FALSE;
      }
      bytes--;
    }
    header_len += le_long;
  }

  /* now let's look for the data chunk.  Following the string "data" is the
     length of the following WAVE data. */
  for (;;) {
    if (!read_tag(info->input,tag) || !read_le_long(info->input,&le_long)) {
      st_warning("reached end of file looking for data tag while processing file: [%s]",info->filename);
      return FALSE;
    }

    st_debug1("found chunk: [%c%c%c%c] with length: %lu",tag[0],tag[1],tag[2],tag[3],le_long);

    header_len += 8;

    if (!tagcmp(tag,(unsigned char *)WAVE_DATA))
      break;

    bytes = le_long;

    while (bytes > 0) {
      if (fread(buf,1,1,info->input) != 1) {
        st_warning("reached end of file jumping ahead %lu bytes when looking for data tag while processing file: [%s]",le_long,info->filename);
        return FALSE;
      }
      bytes--;
    }

    header_len += le_long;
  }

  info->data_size = le_long;

  info->header_size = header_len;

  if (!do_header_kluges(NULL,info))
    return FALSE;

  info->rate = ((wint)info->samples_per_sec * (wint)info->channels * (wint)info->bits_per_sample) / 8;

  info->total_size = info->chunk_size + 8;

  /* per RIFF specs, data chunk is always an even number of bytes - a NULL pad byte should be present when data size is odd */
  info->padded_data_size = info->data_size;
  if (PROB_ODD_SIZED_DATA(info))
    info->padded_data_size++;

  info->extra_riff_size = info->total_size - (info->padded_data_size + info->header_size);

  info->length = info->data_size / info->rate;
  info->exact_length = (double)info->data_size / (double)info->rate;

  if (info->channels == CD_CHANNELS &&
      info->bits_per_sample == CD_BITS_PER_SAMPLE &&
      info->samples_per_sec == CD_SAMPLES_PER_SEC &&
/*
  removed, since this doesn't seem to matter
      info->block_align == CD_BLOCK_ALIGN &&
*/
      info->avg_bytes_per_sec == CD_RATE &&
      info->rate == CD_RATE)
  {
    if (info->data_size < CD_MIN_BURNABLE_SIZE)
      info->problems |= PROBLEM_CD_BUT_TOO_SHORT;
    if (info->data_size % CD_BLOCK_SIZE != 0)
      info->problems |= PROBLEM_CD_BUT_BAD_BOUND;
  }
  else
    info->problems |= PROBLEM_NOT_CD_QUALITY;

  if (info->header_size != CANONICAL_HEADER_SIZE)
    info->problems |= PROBLEM_HEADER_NOT_CANONICAL;

  if (info->data_size > info->total_size - (wlong)info->header_size)
    info->problems |= PROBLEM_HEADER_INCONSISTENT;

  if ((info->data_size % info->block_align) != 0)
    info->problems |= PROBLEM_DATA_NOT_ALIGNED;

  if (info->input_format && !info->input_format->is_compressed && !info->input_format->is_translated) {
    if (info->total_size < (info->actual_size - info->id3v2_tag_size))
      info->problems |= PROBLEM_JUNK_APPENDED;

    if (info->total_size > (info->actual_size - info->id3v2_tag_size))
      info->problems |= PROBLEM_MAY_BE_TRUNCATED;
  }

  if (info->extra_riff_size > 0)
    info->problems |= PROBLEM_EXTRA_CHUNKS;

  length_to_str(info);

  /* header looks ok */
  return TRUE;
}

wave_info *new_wave_info(char *filename)
/* if filename is NULL, return a fresh wave_info struct with all data zero'd out.
 * Otherwise, check that the file referenced by filename exists, is readable, and
 * contains a valid WAVE header - if so, return a wave_info struct filled out with
 * the values in the WAVE header, otherwise return NULL.
 */
{
  int i,bytes;
  FILE *f;
  wave_info *info;
  unsigned char buf[8];
  char msg[BUF_SIZE],tmp[BUF_SIZE];

  if (NULL == (info = malloc(sizeof(wave_info)))) {
    st_warning("could not allocate memory for WAVE info struct");
    goto invalid_wave_data;
  }

  /* set defaults */
  memset((void *)info,0,sizeof(wave_info));

  if (NULL == filename)
    return info;

  info->filename = filename;

  if (!is_valid_file(info))
    goto invalid_wave_data;

  /* check which format module (if any) handles this file */
  for (i=0;st_formats[i];i++) {
    if (!st_formats[i]->supports_input)
      continue;

    if (st_formats[i]->is_our_file) {
      /* format defines its own checking function - use it */
      if (!st_formats[i]->is_our_file(info->filename))
        continue;
    }
    else {
      /* otherwise, check for format-defined magic string at a predefined offset (if defined) */
      if (!check_for_magic(info->filename,st_formats[i]->magic,st_formats[i]->magic_offset))
        continue;
    }

    /* found a format that claims to handle this file */
    info->input_format = st_formats[i];

    /* check if file contains an ID3v2 tag, and set flag accordingly */
    if (NULL == (f = open_input_internal(info->filename,&info->file_has_id3v2_tag,&info->id3v2_tag_size))) {
      st_warning("open failed while setting ID3v2 flag for file: [%s]",info->filename);
      goto invalid_wave_data;
    }

    fclose(f);

    /* make sure the file can be opened by the output format - this skips over any ID3v2 tags in the stream */
    if (!open_input_stream(info)) {
      st_debug1("input file could not be opened for streaming input by format: [%s]",st_formats[i]->name);
      goto invalid_wave_data;
    }

    /* make sure we can read data from the output format (primarily to ensure the decoder is sending us data) */
    if (1 != fread(&buf,1,1,info->input)) {
      st_snprintf(msg,BUF_SIZE,"failed to read data from input file using format: [%s]\n",info->input_format->name);

      st_snprintf(tmp,BUF_SIZE,"+ you may not have permission to read file: [%s]\n",info->filename);
      strcat(msg,tmp);

      if (info->input_format->decoder) {
        st_snprintf(tmp,BUF_SIZE,"+ arguments may be incorrect for decoder: [%s]\n",info->input_format->decoder);
        strcat(msg,tmp);

        strcat(msg,"+ verify that the decoder is installed and in your PATH\n");

        if (info->file_has_id3v2_tag) {
          strcat(msg,"+ removing the ID3v2 tag from this file may fix this\n");
        }
      }

      strcat(msg,"+ this file may be unsupported, truncated or corrupt");

      st_warning(msg);

      goto invalid_wave_data;
    }

    close_input_stream(info);
    info->input = NULL;

    /* reopen as input stream - this skips over any ID3v2 tags in the stream */
    if (!open_input_stream(info))
      goto invalid_wave_data;

    /* finally, make sure a proper WAVE header is being sent */
    if (!verify_wav_header(info))
      goto invalid_wave_data;

    close_input_stream(info);
    info->input = NULL;

    /* success */
    return info;
  }

  /* if we got here, no file format modules claimed to handle the file */

  st_warning("none of the builtin format modules handle input file: [%s]",info->filename);

  if ((f = open_input_internal(info->filename,&info->file_has_id3v2_tag,&info->id3v2_tag_size))) {
    bytes = read_n_bytes(f,buf,4,NULL);
    buf[bytes] = 0;

    for (i=0;i<bytes;i++) {
      if (!isprint((unsigned char)buf[i]))
        buf[i] = '?';
    }

    if (info->file_has_id3v2_tag)
      st_debug1("after skipping %d-byte ID3v2 tag, found %d-byte magic header 0x%08X [%s] in file: [%s]",info->id3v2_tag_size,i,uchar_to_ulong_be(buf),buf,info->filename);
    else
      st_debug1("found %d-byte magic header 0x%08X [%s] in file: [%s]",i,uchar_to_ulong_be(buf),buf,info->filename);

    fclose(f);
  }

invalid_wave_data:

  if (info) {
    if (info->input) {
      close_input_stream(info);
      info->input = NULL;
    }
    st_free(info);
  }

  return NULL;
}

void make_canonical_header(unsigned char *header,wave_info *info)
/* constructs a canonical WAVE header from the values in the wave_info struct */
{
  if (NULL == header)
    return;

  tagcpy(header,(unsigned char *)WAVE_RIFF);
  ulong_to_uchar_le(header+4,info->chunk_size);
  tagcpy(header+8,(unsigned char *)WAVE_WAVE);
  tagcpy(header+12,(unsigned char *)WAVE_FMT);
  ulong_to_uchar_le(header+16,0x00000010);
  ushort_to_uchar_le(header+20,info->wave_format);
  ushort_to_uchar_le(header+22,info->channels);
  ulong_to_uchar_le(header+24,info->samples_per_sec);
  ulong_to_uchar_le(header+28,info->avg_bytes_per_sec);
  ushort_to_uchar_le(header+32,info->block_align);
  ushort_to_uchar_le(header+34,info->bits_per_sample);
  tagcpy(header+36,(unsigned char *)WAVE_DATA);
  ulong_to_uchar_le(header+40,info->data_size);
}

char *format_to_str(wshort format)
{
  switch (format) {
    case WAVE_FORMAT_UNKNOWN:
      return "Microsoft Official Unknown";
    case WAVE_FORMAT_PCM:
      return "Microsoft PCM";
    case WAVE_FORMAT_ADPCM:
      return "Microsoft ADPCM";
    case WAVE_FORMAT_IEEE_FLOAT:
      return "IEEE Float";
    case WAVE_FORMAT_ALAW:
      return "Microsoft A-law";
    case WAVE_FORMAT_MULAW:
      return "Microsoft U-law";
    case WAVE_FORMAT_OKI_ADPCM:
      return "OKI ADPCM format";
    case WAVE_FORMAT_IMA_ADPCM:
      return "IMA ADPCM";
    case WAVE_FORMAT_DIGISTD:
      return "Digistd format";
    case WAVE_FORMAT_DIGIFIX:
      return "Digifix format";
    case WAVE_FORMAT_DOLBY_AC2:
      return "Dolby AC2";
    case WAVE_FORMAT_GSM610:
      return "GSM 6.10";
    case WAVE_FORMAT_ROCKWELL_ADPCM:
      return "Rockwell ADPCM";
    case WAVE_FORMAT_ROCKWELL_DIGITALK:
      return "Rockwell DIGITALK";
    case WAVE_FORMAT_G721_ADPCM:
      return "G.721 ADPCM";
    case WAVE_FORMAT_G728_CELP:
      return "G.728 CELP";
    case WAVE_FORMAT_MPEG:
      return "MPEG";
    case WAVE_FORMAT_MPEGLAYER3:
      return "MPEG Layer 3";
    case WAVE_FORMAT_G726_ADPCM:
      return "G.726 ADPCM";
    case WAVE_FORMAT_G722_ADPCM:
      return "G.722 ADPCM";
  }
  return "Unknown";
}

void put_chunk_size(unsigned char *header,unsigned long new_chunk_size)
/* replaces the chunk size at beginning of the wave header */
{
  if (NULL == header)
    return;

  ulong_to_uchar_le(header+4,new_chunk_size);
}

void put_data_size(unsigned char *header,int header_size,unsigned long new_data_size)
/* replaces the size reported in the "data" chunk of the wave header with the new size -
   also updates chunk size at beginning of the wave header */
{
  if (NULL == header)
    return;

  ulong_to_uchar_le(header+header_size-4,new_data_size);
  put_chunk_size(header,new_data_size+header_size-8);
}
