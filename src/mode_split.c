/*  mode_split.c - split mode module
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
#include "mode.h"

CVSID("$Id: mode_split.c,v 1.145 2009/03/18 22:25:00 jason Exp $")

static bool split_main(int,char **);
static void split_help(void);

mode_module mode_split = {
  "split",
  "shnsplit",
  "Splits PCM WAVE data from one file into multiple files",
  CVSIDSTR,
  TRUE,
  split_main,
  split_help
};

#define SPLIT_PREFIX "split-track"
#define SPLIT_MAX_PIECES 256
#define SPLIT_NUM_FORMAT "%02d"

enum {
  SPLIT_INPUT_UNKNOWN,
  SPLIT_INPUT_RAW,
  SPLIT_INPUT_CUE
};

static bool input_is_cd_quality = FALSE;
static int numfiles = 0;
static int numextracted = 0;
static int offset = 1;
static int input_type = SPLIT_INPUT_UNKNOWN;
static char *num_format = SPLIT_NUM_FORMAT;
static char *split_point_file = NULL;
static char *repeated_split_point = NULL;
static char *leadin = NULL;
static char *leadout = NULL;
static char *extract_tracks = NULL;
static char *manipulate_chars = NULL;

typedef struct _cue_info {
  /* global */
  char *format;
  char artist[FILENAME_SIZE];
  char album[FILENAME_SIZE];
  /* per-track */
  char artists[SPLIT_MAX_PIECES][FILENAME_SIZE];
  char titles[SPLIT_MAX_PIECES][FILENAME_SIZE];
  char filenames[SPLIT_MAX_PIECES][FILENAME_SIZE];
  /* misc vars */
  int  trackno;
  bool in_global_section;
  bool in_new_track_section;
} cue_info;

static cue_info cueinfo;

static wave_info *files[SPLIT_MAX_PIECES];
static bool extract_track[SPLIT_MAX_PIECES];

static void split_help()
{
  int i;

  st_info("Usage: %s [OPTIONS] [file]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c num  start counting from num when naming output files (default is 1)\n");
  st_info("  -e len  prefix each track with len amount of lead-in from previous track (*)\n");
  st_info("  -f file read split point data from file\n");
  st_info("  -h      show this help screen\n");
  st_info("  -l len  split input file into files of length len (*)\n");
  st_info("  -m str  specify character manipulation string (alternating from/to)\n");
  st_info("  -n fmt  specify file count output format (default is %s -- ",SPLIT_NUM_FORMAT);
  for (i=0;i<3;i++) {
    st_info(SPLIT_NUM_FORMAT ", ",i+1);
  }
  st_info("...)\n");
  st_info("  -t fmt  name output files in user-specified format based on CUE sheet fields.\n");
  st_info("          (%%p = performer, %%a = album, %%t = track title, %%n = track number)\n");
  st_info("  -u len  postfix each track with len amount of lead-out from next track (*)\n");
  st_info("  -x list only extract tracks in list (comma-separated, may contain ranges)\n");
  st_info("\n");
  st_info("          (*) len must be in bytes, m:ss, m:ss.ff or m:ss.nnn format\n");
  st_info("\n");
  st_info("If no split point file is given, then split points are read from the terminal.\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = CURRENT_DIR;
  st_ops.output_prefix = SPLIT_PREFIX;
  cueinfo.format = NULL;

  while ((c = st_getopt(argc,argv,"c:e:f:l:n:m:t:u:x:")) != -1) {
    switch (c) {
      case 'c':
        if (NULL == optarg)
          st_error("missing starting count");
        offset = atoi(optarg);
        if (offset < 0)
          st_help("starting count cannot be negative");
        break;
      case 'e':
        if (NULL == optarg)
          st_error("missing lead-in");
        leadin = optarg;
        break;
      case 'f':
        if (NULL == optarg)
          st_error("missing split point file");
        split_point_file = optarg;
        break;
      case 'l':
        if (NULL == optarg)
          st_error("missing repeated split point");
        repeated_split_point = optarg;
        break;
      case 'm':
        if (NULL == optarg)
          st_error("missing character manipulation list");
        manipulate_chars = optarg;
        break;
      case 'n':
        if (NULL == optarg)
          st_error("missing number output format");
        num_format = optarg;
        break;
      case 't':
        if (NULL == optarg)
          st_error("missing cue format");
        cueinfo.format = optarg;
        /* override existing output prefix, if not changed by the user */
        if (!strcmp(st_ops.output_prefix,SPLIT_PREFIX))
          st_ops.output_prefix = "";
        break;
      case 'u':
        if (NULL == optarg)
          st_error("missing lead-out");
        leadout = optarg;
        break;
      case 'x':
        if (NULL == optarg)
          st_error("missing track numbers to extract");
        extract_tracks = optarg;
        break;
    }
  }

  if (optind >= argc && !split_point_file)
    st_help("if file to be split is not given, then a split point file must be specified");

  *first_arg = optind;
}

static void adjust_for_leadinout(wlong leadin_bytes,wlong leadout_bytes)
{
  int current,pass;
  wlong this_leadin,this_leadout;

  /* adjust WAVE data size for each file based on any lead-in/lead-outs specified */
  for (pass=0;pass<2;pass++) {
    for (current=0;current<numfiles;current++) {
      this_leadin = leadin_bytes;
      this_leadout = leadout_bytes;
      if (0 == current)
        this_leadin = 0;
      if (numfiles - 1 == current)
        this_leadout = 0;

      switch (pass) {
        case 0:
          /* first pass: check sizes */
          if (0 != current && files[current-1]->data_size < leadin_bytes)
            st_error("lead-in length exceeds length of one or more files");
          if (numfiles - 1 != current && files[current+1]->data_size < leadout_bytes)
            st_error("lead-out length exceeds length of one or more files");
          break;
        case 1:
          /* second pass: actually adjust data sizes */
          files[current]->new_data_size = files[current]->data_size;
          files[current]->data_size += this_leadin + this_leadout;
          break;
        default:
          break;
      }
    }
  }
}

static bool split_file(wave_info *info)
{
  unsigned char header[CANONICAL_HEADER_SIZE];
  char outfilename[FILENAME_SIZE],filenum[FILENAME_SIZE];
  int current;
  wint discard,bytes;
  bool success;
  wlong leadin_bytes, leadout_bytes, bytes_to_xfer;
  progress_info proginfo;

  success = FALSE;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Splitting";
  proginfo.clause = "-->";
  proginfo.filename1 = info->filename;
  proginfo.filedesc1 = info->m_ss;
  proginfo.filename2 = NULL;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = 0;

  if (!open_input_stream(info)) {
    prog_error(&proginfo);
    st_error("could not reopen input file: [%s]",info->filename);
  }

  discard = info->header_size;
  while (discard > 0) {
    bytes = min(discard,CANONICAL_HEADER_SIZE);
    if (read_n_bytes(info->input,header,bytes,NULL) != bytes) {
      prog_error(&proginfo);
      st_error("error while discarding %d-byte WAVE header",info->header_size);
    }
    discard -= bytes;
  }

  leadin_bytes = (leadin) ? smrt_parse((unsigned char *)leadin,info) : 0;
  leadout_bytes = (leadout) ? smrt_parse((unsigned char *)leadout,info) : 0;
  adjust_for_leadinout(leadin_bytes,leadout_bytes);

  for (current=0;current<numfiles;current++) {
    if (SPLIT_INPUT_CUE == input_type && cueinfo.format) {
      create_output_filename(cueinfo.filenames[current],"",outfilename);
    }
    else {
      st_snprintf(filenum,8,num_format,current+offset);
      create_output_filename(filenum,"",outfilename);
    }

    files[current]->chunk_size = files[current]->data_size + CANONICAL_HEADER_SIZE - 8;
    files[current]->channels = info->channels;
    files[current]->samples_per_sec = info->samples_per_sec;
    files[current]->avg_bytes_per_sec = info->avg_bytes_per_sec;
    files[current]->block_align = info->block_align;
    files[current]->bits_per_sample = info->bits_per_sample;
    files[current]->wave_format = info->wave_format;
    files[current]->rate = info->rate;
    files[current]->length = files[current]->data_size / (wlong)info->rate;
    files[current]->exact_length = (double)files[current]->data_size / (double)info->rate;
    files[current]->total_size = files[current]->chunk_size + 8;

    length_to_str(files[current]);

    proginfo.filedesc2 = files[current]->m_ss;
    proginfo.bytes_total = files[current]->total_size;

    if (extract_track[current]) {
      proginfo.prefix = "Splitting";
      proginfo.filename2 = outfilename;

      if (NULL == (files[current]->output = open_output_stream(outfilename,&files[current]->output_proc))) {
        prog_error(&proginfo);
        st_error("could not open output file");
      }
    }
    else {
      proginfo.prefix = "Skipping ";
      proginfo.filename2 = NULLDEVICE;

      if (NULL == (files[current]->output = open_output(NULLDEVICE))) {
        prog_error(&proginfo);
        st_error("while skipping track %d: could not open output file: [%s]",current+1,NULLDEVICE);
      }

      files[current]->output_proc.pid = NO_CHILD_PID;
    }

    if (PROB_ODD_SIZED_DATA(files[current]))
      files[current]->chunk_size++;

    make_canonical_header(header,files[current]);

    prog_update(&proginfo);

    if (write_n_bytes(files[current]->output,header,CANONICAL_HEADER_SIZE,&proginfo) != CANONICAL_HEADER_SIZE) {
      prog_error(&proginfo);
      st_warning("error while writing %d-byte WAVE header",CANONICAL_HEADER_SIZE);
      goto cleanup;
    }

    /* if this is not the first file, finish up writing previous file, and simultaneously start writing to current file */
    if (0 != current) {
      /* write overlapping lead-in/lead-out data to both previous and current files */
      if (transfer_n_bytes2(info->input,files[current]->output,files[current-1]->output,leadin_bytes+leadout_bytes,&proginfo) != leadin_bytes+leadout_bytes) {
        prog_error(&proginfo);
        st_warning("error while transferring %ld bytes of lead-in/lead-out",leadin_bytes+leadout_bytes);
        goto cleanup;
      }

      /* pad and close previous file */
      if (PROB_ODD_SIZED_DATA(files[current-1]) && (1 != write_padding(files[current-1]->output,1,&proginfo))) {
        prog_error(&proginfo);
        st_warning("error while NULL-padding odd-sized data chunk");
        goto cleanup;
      }

      close_output(files[current-1]->output,files[current-1]->output_proc);
    }

    /* transfer unique non-overlapping data from input file to current file */
    bytes_to_xfer = files[current]->new_data_size;
    if (0 != current)
      bytes_to_xfer -= leadout_bytes;
    if (numfiles - 1 != current)
      bytes_to_xfer -= leadin_bytes;

    if (transfer_n_bytes(info->input,files[current]->output,bytes_to_xfer,&proginfo) != bytes_to_xfer) {
      prog_error(&proginfo);
      st_warning("error while transferring %ld bytes of data",bytes_to_xfer);
      goto cleanup;
    }

    /* if this is the last file, close it */
    if (numfiles - 1 == current) {
      /* pad and close current file */
      if (PROB_ODD_SIZED_DATA(files[current]) && (1 != write_padding(files[current]->output,1,&proginfo))) {
        prog_error(&proginfo);
        st_warning("error while NULL-padding odd-sized data chunk");
        goto cleanup;
      }

      close_output(files[current]->output,files[current]->output_proc);
    }

    prog_success(&proginfo);
  }

  close_input_stream(info);

  success = TRUE;

cleanup:
  if (!success) {
    close_output(files[current]->output,files[current]->output_proc);
    remove_file(outfilename);
    st_error("failed to split file");
  }

  return success;
}

static void create_new_splitfile()
{
  if (SPLIT_MAX_PIECES - 1 == numfiles)
    st_error("too many split files would be created -- maximum is %d",SPLIT_MAX_PIECES);

  if (NULL == (files[numfiles] = new_wave_info(NULL)))
    st_error("could not allocate memory for split points array");
}

static void adjust_splitfile(int whichfile)
{
  if (input_is_cd_quality) {
    if (files[whichfile]->data_size < CD_MIN_BURNABLE_SIZE) {
      files[whichfile]->problems |= PROBLEM_CD_BUT_TOO_SHORT;
      if (extract_track[whichfile])
        st_warning("file %d will be too short to be burned%s",whichfile+1,
                   (files[whichfile]->data_size % CD_BLOCK_SIZE)?", and will not be cut on a sector boundary":"");
      if (files[whichfile]->data_size % CD_BLOCK_SIZE)
        files[whichfile]->problems |= PROBLEM_CD_BUT_BAD_BOUND;
    }
    else if (files[whichfile]->data_size % CD_BLOCK_SIZE) {
      files[whichfile]->problems |= PROBLEM_CD_BUT_BAD_BOUND;
      if (extract_track[whichfile])
        st_warning("file %d will not be cut on a sector boundary",whichfile+1);
    }
  }
  else {
    files[whichfile]->problems |= PROBLEM_NOT_CD_QUALITY;
  }
}

static void xlate_chars(char *str,char *xlstr)
{
  int i;
  char *p;

  if (!str || !xlstr)
    return;

  for (i=0;xlstr[i] && xlstr[i+1];i+=2) {
    for (p=str;*p;p++) {
      if (xlstr[i] == *p) {
        *p = xlstr[i+1];
      }
    }
  }
}

static void cue_sprintf(int tracknum,char *filename)
{
  char *p,c[2],num[8],psc[4];

  p = cueinfo.format;
  c[1] = 0;
  strcpy(filename,"");
  st_snprintf(psc,4,"%c",PATHSEPCHAR);

  while (*p) {
    if ('%' == *p) {
      if ('a' == *(p+1)) {
        strcat(filename,cueinfo.album);
        p+=2;
        continue;
      }
      if ('p' == *(p+1)) {
        strcat(filename,cueinfo.artists[tracknum]);
        p+=2;
        continue;
      }
      if ('t' == *(p+1)) {
        strcat(filename,cueinfo.titles[tracknum]);
        p+=2;
        continue;
      }
      if ('n' == *(p+1)) {
        st_snprintf(num,8,num_format,tracknum+offset);
        strcat(filename,num);
        p+=2;
        continue;
      }
    }
    c[0] = *p;
    strcat(filename,c);
    p++;
  }

  xlate_chars(filename,manipulate_chars);

  if (extract_track[tracknum] && strstr(filename,psc)) {
    st_warning("converting illegal path character '%c' to '-' in CUE-originated filename: [%s]",PATHSEPCHAR,filename);
    st_snprintf(psc,4,"%c%c",PATHSEPCHAR,'-');
    xlate_chars(filename,psc);
  }
}

static void extract(unsigned char *data)
/* parse out raw lengths, or lengths on INDEX lines of cue sheets */
{
  unsigned char *p,*q;

  trim((char *)data);

  if (0 == strlen((const char *)data))
    return;

  if (strstr((const char *)data,"INDEX") && (NULL == strstr((const char *)data,":"))) {
    strcpy((char *)data,"");
    return;
  }

  /* isolate length */
  p = data + strlen((const char *)data) - 1;

  while (p >= data && (!isdigit(*p) && ':' != *p && '.' != *p)) {
    *p = 0;
    p--;
  }

  while (p >= data && (isdigit(*p) || ':' == *p || '.' == *p))
    p--;

  p++;

  /* copy length to beginning of buffer */
  q = data;

  while (*p) {
    *q = *p;
    p++;
    q++;
  }

  *q = 0;

  /* convert m:ss:ff to m:ss.ff */

  p = (unsigned char *)strrchr((const char *)data,':');
  q = (unsigned char *)strchr((const char *)data,':');

  if (p && q && p != q)
    *p = '.';
}

static void get_cue_field(unsigned char *field,unsigned char *line,char *buf)
{
  char *p,tmp[FILENAME_SIZE];

  strcpy(tmp,(const char *)line);
  trim(tmp);

  /* skip over this keyword to next token */
  p = strstr(tmp,(const char *)field);
  p += strlen((char *)field);
  while (' ' == *p || '\t' == *p)
    p++;

  /* check for quotes, and remove them if present */

  if ('"' == *p)
    p++;
  if ('"' == p[strlen(p)-1])
    p[strlen(p)-1] = 0;

  strcpy(buf,p);
}

static bool handle_cue_keyword(unsigned char *keyword,unsigned char *line)
{
  /* REM lines are always ignored */
  if (!strcmp((char *)keyword,"REM"))
    return FALSE;

  /* TRACK lines indicate beginning of possible TITLES for the tracks */
  if (!strcmp((char *)keyword,"TRACK")) {
    cueinfo.in_global_section = FALSE;
    cueinfo.trackno++;
    return FALSE;
  }

  /* TITLE lines are parsed for album names and/or song titles */
  if (!strcmp((char *)keyword,"TITLE")) {

    /* TITLE keywords in global section indicate album name */
    if (cueinfo.in_global_section) {
      get_cue_field(keyword,line,cueinfo.album);
      return FALSE;
    }

    /* TITLE keywords in track sections indicate song title */
    get_cue_field(keyword,line,cueinfo.titles[cueinfo.trackno-1]);
    return FALSE;
  }

  /* PERFORMER lines are parsed for artist name */
  if (!strcmp((char *)keyword,"PERFORMER")) {

    /* PERFORMER keywords in global section indicate artist name - will override track artist names where not defined */
    if (cueinfo.in_global_section) {
      get_cue_field(keyword,line,cueinfo.artist);
      return FALSE;
    }

    /* PERFORMER keywords in track sections indicate artist name */
    get_cue_field(keyword,line,cueinfo.artists[cueinfo.trackno-1]);
    return FALSE;
  }

  /* INDEX 01 lines are parsed for track timings */
  if ((!strcmp((char *)keyword,"INDEX")) && (strstr((const char *)line,"INDEX 01")))
    return TRUE;

  /* everything else is skipped */
  return FALSE;
}

static void get_cue_keyword(unsigned char *line,unsigned char *keyword)
{
  unsigned char *p,buf[BUF_SIZE];

  p = line;

  /* skip over any binary junk at the beginning (including Unicode BOMs) */
  while (*p && !isprint(*p))
    p++;

  strcpy((char *)buf,(const char *)p);

  p = (unsigned char *)strtok((char *)buf," \t");

  strcpy((char *)keyword,(p)?(const char *)p:"");
}

static bool get_length_token(FILE *input,unsigned char *token)
{
  unsigned char keyword[BUF_SIZE];
  char *p;

  strcpy((char *)token,"");

  if (feof(input))
    return FALSE;

  switch (input_type) {
    case SPLIT_INPUT_UNKNOWN:
      p = fgets((char *)token,BUF_SIZE-1,input);
      if (!p && feof(input))
        return FALSE;
      get_cue_keyword(token,keyword);
      if (
           /* CUE global keywords */
           (!strcmp((char *)keyword,"FILE")) ||
           (!strcmp((char *)keyword,"CATALOG")) ||
           (!strcmp((char *)keyword,"CDTEXTFILE")) ||
           (!strcmp((char *)keyword,"REM")) ||
           /* CD-TEXT keywords */
           (!strcmp((char *)keyword,"TITLE")) ||
           (!strcmp((char *)keyword,"PERFORMER")) ||
           (!strcmp((char *)keyword,"SONGWRITER")) ||
           (!strcmp((char *)keyword,"COMPOSER")) ||
           (!strcmp((char *)keyword,"ARRANGER")) ||
           (!strcmp((char *)keyword,"MESSAGE")) ||
           (!strcmp((char *)keyword,"DISC_ID")) ||
           (!strcmp((char *)keyword,"GENRE")) ||
           (!strcmp((char *)keyword,"TOC_INFO")) ||
           (!strcmp((char *)keyword,"TOC_INFO2")) ||
           (!strcmp((char *)keyword,"UPC_EAN")) ||
           (!strcmp((char *)keyword,"ISRC")) ||
           (!strcmp((char *)keyword,"SIZE_INFO"))
         )
      {
        input_type = SPLIT_INPUT_CUE;
        handle_cue_keyword(keyword,token);
        return get_length_token(input,token);
      }
      else {
        input_type = SPLIT_INPUT_RAW;
        extract(token);
      }
      break;
    case SPLIT_INPUT_RAW:
      p = fgets((char *)token,BUF_SIZE-1,input);
      if (!p && feof(input))
        return FALSE;
      extract(token);
      break;
    case SPLIT_INPUT_CUE:
      p = fgets((char *)token,BUF_SIZE-1,input);
      while (p || !feof(input)) {
        get_cue_keyword(token,keyword);
        if (handle_cue_keyword(keyword,token))
          break;
        p = fgets((char *)token,BUF_SIZE-1,input);
      }
      if (!p && feof(input))
        return FALSE;
      extract(token);
      break;
  }

  return TRUE;
}

static void read_split_points_file(wave_info *info)
{
  FILE *fd;
  unsigned char data[BUF_SIZE];
  wlong current,previous = 0;
  int i,j;
  bool got_token;

  cueinfo.trackno = 0;
  cueinfo.in_global_section = TRUE;
  cueinfo.in_new_track_section = FALSE;
  strcpy(cueinfo.artist,"");
  strcpy(cueinfo.album,"");
  for (i=0;i<SPLIT_MAX_PIECES;i++) {
    strcpy(cueinfo.titles[i],"");
    strcpy(cueinfo.artists[i],"");
  }

  if (split_point_file) {
    if (NULL == (fd = fopen(split_point_file,"rb")))
      st_error("could not open split point file: [%s]",split_point_file);
  }
  else {
    if (isatty(fileno(stdin)))
      st_info("enter split points:\n");
    fd = stdin;
  }

  got_token = get_length_token(fd,data);

  while (got_token || !feof(fd)) {
    current = smrt_parse(data,info);

    if (0 == numfiles && 0 == current) {
      st_warning("discarding initial zero-valued split point");
    }
    else {
      create_new_splitfile();

      files[numfiles]->beginning_byte = current;
      if (files[numfiles]->beginning_byte <= previous)
        st_error("split point %lu is not greater than previous split point %lu",files[numfiles]->beginning_byte,previous);

      files[numfiles]->data_size = files[numfiles]->beginning_byte - previous;

      adjust_splitfile(numfiles);

      if (SPLIT_INPUT_CUE == input_type && 1 == cueinfo.trackno) {
        strcpy(cueinfo.titles[1],cueinfo.titles[0]);
        strcpy(cueinfo.titles[0],"pregap");
        cueinfo.trackno++;
        offset--;
      }

      numfiles++;

      previous = current;
    }

    got_token = get_length_token(fd,data);
  }

  if (NULL == (files[numfiles] = new_wave_info(NULL)))
    st_error("could not allocate memory for split points array");

  numfiles++;

  if (split_point_file)
    fclose(fd);

  if (1 == numfiles)
    st_error("no split points given -- nothing to do");

  if (SPLIT_INPUT_CUE == input_type && cueinfo.format) {
    if (cueinfo.trackno < numfiles)
      st_error("not enough TITLE keywords in CUE sheet to name each output file");

    /* global artist overrides track artist when not defined */
    for (i=0;i<cueinfo.trackno;i++) {
      if (!strcmp(cueinfo.artists[i],""))
        strcpy(cueinfo.artists[i],cueinfo.artist);
    }

    for (i=0;i<cueinfo.trackno;i++)
      cue_sprintf(i,cueinfo.filenames[i]);

    /* make sure there are no duplicate filenames */
    for (i=0;i<cueinfo.trackno;i++) {
      for (j=i+1;j<cueinfo.trackno;j++) {
        if (!strcmp(cueinfo.filenames[i],cueinfo.filenames[j])) {
          st_error("detected duplicate filenames: [%s]",cueinfo.filenames[i]);
        }
      }
    }
  }
}

static void read_split_points_repeated(wave_info *info)
{
  wlong repeated_split_size = smrt_parse((unsigned char *)repeated_split_point,info);
  wlong bytes_left;

  if (repeated_split_size >= info->data_size)
    st_error("repeated split size is not less than data size of input file -- nothing to do");

  bytes_left = info->data_size;

  while (bytes_left > repeated_split_size) {
    create_new_splitfile();

    files[numfiles]->beginning_byte = (numfiles+1) * repeated_split_size;

    files[numfiles]->data_size = repeated_split_size;

    adjust_splitfile(numfiles);

    numfiles++;

    bytes_left -= repeated_split_size;
  }

  if (NULL == (files[numfiles] = new_wave_info(NULL)))
    st_error("could not allocate memory for split points array");

  numfiles++;
}

static void get_extractable_tracks()
{
  int i,start,end;
  char *p,*q,*seps = ",";

  for (i=0;i<numfiles;i++)
    extract_track[i] = FALSE;

  if (extract_tracks) {
    /* extract only tracks in user-specified list */

    p = strtok(extract_tracks,seps);
    while (p) {
      if ((q = strstr(p,"-"))) {
        *q = 0;
        q++;
      }
      start = atoi(p);
      end = (q) ? ((*q) ? atoi(q) : SPLIT_MAX_PIECES) : start;
      if (start > end) {
        i = start;
        start = end;
        end = i;
      }
      if (start < 1)
        st_error("track number to extract (%d) is not in range [1 .. %d]",start,SPLIT_MAX_PIECES);
      if (end > SPLIT_MAX_PIECES)
        st_error("track number to extract (%d) is not in range [1 .. %d]",end,SPLIT_MAX_PIECES);
      start--;
      end--;

      for (i=start;i<=end;i++) {
        extract_track[i] = TRUE;
        numextracted++;
      }

      p = strtok(NULL,seps);
    }

    return;
  }

  /* extract all tracks */
  for (i=0;i<SPLIT_MAX_PIECES;i++) {
    extract_track[i] = TRUE;
    numextracted++;
  }
}

static bool process_file(char *filename)
{
  int i;
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename)))
    st_error("cannot continue due to error(s) shown above");

  input_is_cd_quality = (PROB_NOT_CD(info) ? FALSE : TRUE);

  get_extractable_tracks();

  if (repeated_split_point)
    read_split_points_repeated(info);
  else
    read_split_points_file(info);

  if (files[numfiles-2]->beginning_byte > info->data_size)
    st_error("split points go beyond input file's data size");

  if (files[numfiles-2]->beginning_byte == info->data_size) {
    st_free(files[numfiles-1]);
    numfiles--;
  }
  else {
    files[numfiles-1]->beginning_byte = info->data_size;
    files[numfiles-1]->data_size = info->data_size - files[numfiles-2]->beginning_byte;

    adjust_splitfile(numfiles-1);
  }

  success = split_file(info);

  for (i=0;i<numfiles;i++)
    st_free(files[i]);

  st_free(info);

  return success;
}

static bool process(int argc,char **argv,int start)
{  
  char *filename;
  bool success;

  input_init(start,argc,argv);

  if (!(filename = input_get_filename())) {
    st_error("need exactly one file to process");
  }

  success = process_file(filename);

  return success;
}

static bool split_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
