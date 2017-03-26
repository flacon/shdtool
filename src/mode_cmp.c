/*  mode_cmp.c - cmp mode module
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

#include "mode.h"

CVSID("$Id: mode_cmp.c,v 1.91 2009/03/17 17:23:05 jason Exp $")

static bool cmp_main(int,char **);
static void cmp_help(void);

mode_module mode_cmp = {
  "cmp",
  "shncmp",
  "Compares PCM WAVE data in two files",
  CVSIDSTR,
  FALSE,
  cmp_main,
  cmp_help
};

#define CMP_MATCH_SIZE 2352  /* arbitrary number of bytes that must match before attempting
                                to check whether shifted data in the files is identical */
static bool align = FALSE;
static bool list = FALSE;
static int fuzz = 0;
static wlong shift_secs = 3;

static void cmp_help()
{
  st_info("Usage: %s [OPTIONS] [file1 file2]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c secs check the first secs seconds of data for byte shift (default is %d)\n",shift_secs);
  st_info("  -f fuzz fuzz factor: allow up to fuzz mismatches when detecting a byte-shift\n");
  st_info("  -h      show this help screen\n");
  st_info("  -l      list offsets and values of all differing bytes\n");
  st_info("  -s      check if WAVE data in the files is identical modulo a byte-shift\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  while ((c = st_getopt(argc,argv,"c:f:ls")) != -1) {
    switch (c) {
      case 'c':
        if (NULL == optarg)
          st_help("missing seconds for byte-shift comparison");
        shift_secs = atoi(optarg);
        if (shift_secs <= 0)
          st_help("seconds for byte-shift comparison must be positive");
        break;
      case 'f':
        if (NULL == optarg)
          st_help("missing fuzz factor");
        fuzz = atoi(optarg);
        if (fuzz < 0)
          st_help("fuzz factor cannot be negative");
        break;
      case 'l':
        list = TRUE;
        break;
      case 's':
        align = TRUE;
        break;
    }
  }

  if (fuzz > 0 && !align)
    st_help("fuzz factor can only be used with byte-shift");

  if (optind != argc && optind != argc - 2)
    st_help("need exactly two files to process");

  *first_arg = optind;
}

static void check_headers(wave_info *info1,wave_info *info2,int shift)
{
  int data_size1,data_size2,real_shift = (shift < 0) ? -shift : shift;

  data_size1 = info1->data_size;
  data_size2 = info2->data_size;

  if (shift > 0)
    data_size1 -= real_shift;
  else if (shift < 0)
    data_size2 -= real_shift;

  if (info1->wave_format != info2->wave_format)
    st_error("WAVE format differs between these files");

  if (info1->channels != info2->channels)
    st_error("number of channels differs between these files");

  if (info1->samples_per_sec != info2->samples_per_sec)
    st_error("samples per second differs between these files");

  if (info1->avg_bytes_per_sec != info2->avg_bytes_per_sec)
    st_error("average bytes per second differs between these files");

  if (info1->bits_per_sample != info2->bits_per_sample)
    st_error("bits per sample differs between these files");

  if (info1->block_align != info2->block_align)
    st_warning("block align differs between these files");

  if (data_size1 != data_size2)
    st_warning("WAVE data size differs between these files -- will check up to smaller size");
}

static int memfuzzycmp(unsigned char *str1,unsigned char *str2,int len,int fuzz)
{
  int i,firstbad = -1,badcount = 0;

  for (i=0;i<len;i++) {
    if (str1[i] != str2[i]) {
      if (0 == fuzz)
        return i;
      if (firstbad < 0)
        firstbad = i;
      badcount++;
      if (badcount > fuzz)
        return firstbad;
    }
  }

  return -1;
}

static void open_file(wave_info *info)
{
  if (!open_input_stream(info))
    st_error("could not reopen input file: [%s]",info->filename);
}

static bool cmp_files(wave_info *info1,wave_info *info2,int shift)
{
  unsigned char *buf1,*buf2;
  wlong bytes_to_check,
        bytes_checked = 0,
        bytes,
        shifted_data_size1 = info1->data_size,
        shifted_data_size2 = info2->data_size,
        xfer_size;
  int offset, real_shift, i;
  bool differed = FALSE, did_l_header = FALSE, success;
  progress_info proginfo;

  success = FALSE;

  check_headers(info1,info2,shift);

  proginfo.initialized = FALSE;
  proginfo.prefix = "Comparing";
  proginfo.clause = "and";
  proginfo.filename1 = info1->filename;
  proginfo.filedesc1 = info1->m_ss;
  proginfo.filename2 = info2->filename;
  proginfo.filedesc2 = info2->m_ss;
  proginfo.bytes_total = 1;

  prog_update(&proginfo);

  xfer_size = max(XFER_SIZE,(shift_secs * info1->rate));

  /* kluge to work around free(buf2) dumping core if malloc()'d separately */
  if (NULL == (buf1 = malloc(2 * xfer_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_error("could not allocate %d-byte comparison buffer",xfer_size);
  }

  buf2 = buf1 + xfer_size;

  real_shift = (shift < 0) ? -shift : shift;

  open_file(info1);
  open_file(info2);

  discard_header(info1);
  discard_header(info2);

  if (shift > 0) {
    if (read_n_bytes(info1->input,buf1,real_shift,NULL) != real_shift) {
      prog_error(&proginfo);
      st_error("error while shifting %d bytes from file: [%s]",real_shift,info1->filename);
    }

    shifted_data_size1 -= real_shift;
  }
  else if (shift < 0) {
    if (read_n_bytes(info2->input,buf2,real_shift,NULL) != real_shift) {
      prog_error(&proginfo);
      st_error("error while shifting %d bytes from file: [%s]",real_shift,info2->filename);
    }

    shifted_data_size2 -= real_shift;
  }

  bytes_to_check = min(shifted_data_size1,shifted_data_size2);

  proginfo.filedesc1 = info1->m_ss;
  proginfo.filedesc2 = info2->m_ss;
  proginfo.bytes_total = bytes_to_check;

  while (bytes_to_check > 0) {
    bytes = min(bytes_to_check,xfer_size);
    if (read_n_bytes(info1->input,buf1,(int)bytes,&proginfo) != (int)bytes) {
      prog_error(&proginfo);
      st_error("error while reading %d bytes from file: [%s]",(int)bytes,info1->filename);
    }

    if (read_n_bytes(info2->input,buf2,(int)bytes,NULL) != (int)bytes) {
      prog_error(&proginfo);
      st_error("error while reading %d bytes from file: [%s]",(int)bytes,info2->filename);
    }

    if (-1 != (offset = memfuzzycmp(buf1,buf2,bytes,0))) {
      differed = TRUE;
      if (!list) {
        prog_error(&proginfo);
        st_error("WAVE data differs at byte offset: %lu",bytes_checked+(wlong)offset+1);
      }
      i = 0;
      while (offset >= 0) {
        i += offset + 1;
        if (!did_l_header) {
          prog_error(&proginfo);
          st_info("\n");
          st_info("    offset   1   2\n");
          st_info("   ----------------\n");
          did_l_header = TRUE;
        }
        st_info("%10ld %3d %3d\n",bytes_checked + i,(int)*(buf1 + i - 1),(int)*(buf2 + i - 1));
        offset = memfuzzycmp(buf1 + i,buf2 + i,bytes - i,0);
      }
    }
    bytes_to_check -= bytes;
    bytes_checked += bytes;
  }

  close_input_stream(info1);
  close_input_stream(info2);

  if (!differed) {
    success = TRUE;
    prog_success(&proginfo);
    st_info("\n");
    st_info("%s of these files are identical",(0 == shift) ? "Contents" : "Aligned contents");
    if (shifted_data_size1 != shifted_data_size2)
      st_info(" (up to the first %lu bytes of WAVE data)",min(shifted_data_size1,shifted_data_size2));
    st_info(".\n");
  }
  else {
    success = FALSE;
    st_info("\n");
    st_info("%s of these files differed as indicated above.\n",(0 == shift) ? "Contents" : "Aligned contents");
  }

  st_free(buf1);

  return success;
}

static void open_and_read_beginning(wave_info *info,unsigned char *buf,int bytes)
{
  open_file(info);

  discard_header(info);

  if (read_n_bytes(info->input,buf,(int)bytes,NULL) != (int)bytes)
    st_error("error while reading %d bytes from file: [%s]",(int)bytes,info->filename);

  close_input_stream(info);
}

static bool shift_comparison(wave_info *info1,wave_info *info2)
{
  unsigned char *buf1,*buf2;
  wlong bytes,cmp_size;
  int i,shift = 0,real_shift = 0;
  bool found_possible_shift = FALSE;
  progress_info proginfo;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Scanning";
  proginfo.clause = "and";
  proginfo.filename1 = info1->filename;
  proginfo.filedesc1 = NULL;
  proginfo.filename2 = info2->filename;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = 1;

  prog_update(&proginfo);

  cmp_size = shift_secs * info1->rate;
  bytes = min(min(info1->data_size,info2->data_size),cmp_size);

  if (NULL == (buf1 = malloc(2 * bytes * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_error("could not allocate %d-byte comparison buffer",bytes);
  }

  buf2 = buf1 + bytes;

  open_and_read_beginning(info1,buf1,bytes);
  open_and_read_beginning(info2,buf2,bytes);

  for (i=0;i<bytes-CMP_MATCH_SIZE+1;i++) {
    if (-1 == memfuzzycmp(buf1 + i,buf2,bytes - i,fuzz)) {
      shift = i;
      real_shift = i;
      found_possible_shift = TRUE;
      break;
    }
    if (-1 == memfuzzycmp(buf1,buf2 + i,bytes - i,fuzz)) {
      shift = -i;
      real_shift = i;
      found_possible_shift = TRUE;
      break;
    }
  }

  st_free(buf1);

  if (!found_possible_shift) {
    prog_error(&proginfo);
    st_error("these files do not share identical data within the first %ld bytes.",cmp_size);
  }

  prog_success(&proginfo);

  st_info("\n");

  if (fuzz > 0)
    st_info("With fuzz factor %d, file",fuzz);
  else
    st_info("File");

  if (0 == shift) {
    st_info("s are identical so far.\n");
  }
  else {
    st_info(" the %s file seems to have ",(0 < shift) ? "first" : "second");
    st_info("%d extra bytes (%d extra samples",real_shift,(real_shift + 2)/4);
    if (!PROB_NOT_CD(info1) && !PROB_NOT_CD(info2))
      st_info(", or %d extra sectors",(real_shift+(CD_BLOCK_SIZE/2))/CD_BLOCK_SIZE);
    st_info(")\n");
    st_info("\n");
    st_info("These extra bytes will be ignored in the full comparison.\n");
  }

  st_info("\n");
  st_info("Preparing to do a full comparison...\n");
  st_info("\n");

  return cmp_files(info1,info2,shift);
}

static bool straight_comparison(wave_info *info1,wave_info *info2)
{
  return cmp_files(info1,info2,0);
}

static bool process_files(char *filename1,char *filename2)
{
  wave_info *info1,*info2;
  bool success;

  if (NULL == (info1 = new_wave_info(filename1)))
    return FALSE;

  if (NULL == (info2 = new_wave_info(filename2)))
    return FALSE;

  if (align)
    success = shift_comparison(info1,info2);
  else
    success = straight_comparison(info1,info2);

  st_free(info1);
  st_free(info2);

  return success;
}

static bool process(int argc,char **argv,int start)
{
  char *filename1,*filename2;
  bool success;

  success = FALSE;

  input_init(start,argc,argv);

  if (!(filename1 = input_get_filename()) || !(filename2 = input_get_filename())) {
    st_error("need exactly two files to process");
  }

  success = process_files(filename1,filename2);

  return success;
}

static bool cmp_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
