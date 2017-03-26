/*  wave.h - WAVE data definitions
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

/*
 * $Id: wave.h,v 1.48 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __WAVE_H__
#define __WAVE_H__

#include <stdio.h>
#include "format-types.h"

#define WAVE_RIFF                       "RIFF"
#define WAVE_WAVE                       "WAVE"
#define WAVE_FMT                        "fmt "
#define WAVE_DATA                       "data"

#define AIFF_FORM                       "FORM"
#define AIFF_FORM_TYPE_AIFF             "AIFF"
#define AIFF_FORM_TYPE_AIFC             "AIFC"
#define AIFF_COMM                       "COMM"
#define AIFF_COMPRESSION_NONE           "NONE"
#define AIFF_COMPRESSION_SOWT           "sowt"
#define AIFF_SSND                       "SSND"

#define WAVE_FORMAT_UNKNOWN             (0x0000)
#define WAVE_FORMAT_PCM                 (0x0001)
#define WAVE_FORMAT_ADPCM               (0x0002)
#define WAVE_FORMAT_IEEE_FLOAT          (0x0003)
#define WAVE_FORMAT_ALAW                (0x0006)
#define WAVE_FORMAT_MULAW               (0x0007)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010)
#define WAVE_FORMAT_IMA_ADPCM           (0x0011)
#define WAVE_FORMAT_DIGISTD             (0x0015)
#define WAVE_FORMAT_DIGIFIX             (0x0016)
#define WAVE_FORMAT_DOLBY_AC2           (0x0030)
#define WAVE_FORMAT_GSM610              (0x0031)
#define WAVE_FORMAT_ROCKWELL_ADPCM      (0x003b)
#define WAVE_FORMAT_ROCKWELL_DIGITALK   (0x003c)
#define WAVE_FORMAT_G721_ADPCM          (0x0040)
#define WAVE_FORMAT_G728_CELP           (0x0041)
#define WAVE_FORMAT_MPEG                (0x0050)
#define WAVE_FORMAT_MPEGLAYER3          (0x0055)
#define WAVE_FORMAT_G726_ADPCM          (0x0064)
#define WAVE_FORMAT_G722_ADPCM          (0x0065)

#define CD_BLOCK_SIZE                   (2352)
#define CD_BLOCKS_PER_SEC               (75)
#define CD_BLOCK_ALIGN                  (4)
#define CD_MIN_BURNABLE_SIZE            (705600)
#define CD_CHANNELS                     (2)
#define CD_SAMPLES_PER_SEC              (44100)
#define CD_BITS_PER_SAMPLE              (16)
#define CD_RATE                         (176400)

#define CANONICAL_HEADER_SIZE           (44)

#define PROBLEM_NOT_CD_QUALITY          (0x00000001)
#define PROBLEM_CD_BUT_BAD_BOUND        (0x00000002)
#define PROBLEM_CD_BUT_TOO_SHORT        (0x00000004)
#define PROBLEM_HEADER_NOT_CANONICAL    (0x00000008)
#define PROBLEM_EXTRA_CHUNKS            (0x00000010)
#define PROBLEM_HEADER_INCONSISTENT     (0x00000020)
#define PROBLEM_MAY_BE_TRUNCATED        (0x00000040)
#define PROBLEM_JUNK_APPENDED           (0x00000080)
#define PROBLEM_DATA_NOT_ALIGNED        (0x00000100)

/* macros to determine if files have certain problems */

#define PROB_NOT_CD(f)                  ((f->problems) & (PROBLEM_NOT_CD_QUALITY))
#define PROB_BAD_BOUND(f)               ((f->problems) & (PROBLEM_CD_BUT_BAD_BOUND))
#define PROB_TOO_SHORT(f)               ((f->problems) & (PROBLEM_CD_BUT_TOO_SHORT))
#define PROB_HDR_NOT_CANONICAL(f)       ((f->problems) & (PROBLEM_HEADER_NOT_CANONICAL))
#define PROB_EXTRA_CHUNKS(f)            ((f->problems) & (PROBLEM_EXTRA_CHUNKS))
#define PROB_HDR_INCONSISTENT(f)        ((f->problems) & (PROBLEM_HEADER_INCONSISTENT))
#define PROB_TRUNCATED(f)               ((f->problems) & (PROBLEM_MAY_BE_TRUNCATED))
#define PROB_JUNK_APPENDED(f)           ((f->problems) & (PROBLEM_JUNK_APPENDED))
#define PROB_DATA_NOT_ALIGNED(f)        ((f->problems) & (PROBLEM_DATA_NOT_ALIGNED))
#define PROB_ODD_SIZED_DATA(f)          (f->data_size & 1)

typedef struct _wave_info {
  char *filename,              /* file name of input file                             */
        m_ss[16];              /* length, in m:ss.nnn or m:ss.ff format               */

  wint header_size;            /* length of header, in bytes                          */

  long extra_riff_size;        /* total size of any extra RIFF chunks                 */

  wshort channels,             /* number of channels                                  */
         block_align,          /* block align                                         */
         bits_per_sample,      /* bits per sample                                     */
         wave_format;          /* WAVE data format                                    */

  wlong samples_per_sec,       /* samples per second                                  */
        avg_bytes_per_sec,     /* average bytes per second (should equal rate)        */
        rate,                  /* calculated rate (should equal avg_bytes_per_sec)    */
        length,                /* length of file, in seconds                          */
        data_size,             /* length of data chunk, as reported in the header     */
        padded_data_size,      /* length of data chunk, including possible pad byte   */
        total_size,            /* total length of the WAVE data, header and all       */
        chunk_size,            /* read from header, should be total_size - 8          */
        actual_size,           /* set to the size of input file - this is misleading, */
                               /* since the input file may be compressed              */
        new_data_size,         /* (set and used only in certain modes, ignore)        */
        beginning_byte,        /* (set and used only in certain modes, ignore)        */
        new_beginning_byte;    /* (set and used only in certain modes, ignore)        */

  double exact_length;         /* length of file, in seconds (with ms precision)      */

  unsigned long problems;      /* bitmap of problems found with the file, see above   */

  struct _format_module
                *input_format; /* pointer to the input format module that opens this  */

  FILE *input, *output;        /* input/output file pointers (= NULL, use as needed)  */

  proc_info input_proc;        /* pid (and handle on WIN32) of child input process    */
  proc_info output_proc;       /* pid (and handle on WIN32) of child output process   */

  bool file_has_id3v2_tag;     /* does this file contain an ID3v2 tag?                */
  bool stream_has_id3v2_tag;   /* does the decoded input stream contain an ID3v2 tag? */
  wlong id3v2_tag_size;        /* size of the ID3v2 tag this file contains, if any    */
} wave_info;

/* returns a wave_info struct, filled out with the values of the WAVE data contained in the filename given. */
/* If called with NULL as the argument, then a wave_info struct is returned with all fields zero'd out.     */
wave_info *new_wave_info(char *);

/* constructs a canonical WAVE header from the values in the wave_info struct */
void make_canonical_header(unsigned char *buf,wave_info *info);

/* returns a string corresponding to the WAVE format code given */
char *format_to_str(wshort);

/* replaces the size chunk size at beginning of the wave header */
void put_chunk_size(unsigned char *,unsigned long);

/* replaces the size reported in the "data" chunk of the wave header with the new size -
   also updates chunk size at beginning of the wave header */
void put_data_size(unsigned char *,int,unsigned long);

/* kluges the WAVE header to get correct values when helper programs don't provide them */
bool do_header_kluges(unsigned char *,wave_info *);

/* verifies that data coming in on the associated file descriptor describes a valid WAVE header */
bool verify_wav_header_internal(wave_info *,bool);
#define verify_wav_header(a) verify_wav_header_internal(a,FALSE)

#endif
