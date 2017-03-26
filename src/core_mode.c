/*  core_mode.c - public functions for mode modules
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
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#ifndef WIN32
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include "shntool.h"

CVSID("$Id: core_mode.c,v 1.88 2009/03/30 05:55:33 jason Exp $")

global_opts st_ops;

/* private functions */

#define parse_input_args_cmd(a,b)   parse_args(a,b,FALSE,ARGSRC_CMDLINE)
#define parse_output_args_cmd(a,b)  parse_args(a,b,TRUE,ARGSRC_CMDLINE)
#define parse_input_args_env(a,b)   parse_args(a,b,FALSE,ARGSRC_ENV)
#define parse_output_args_env(a,b)  parse_args(a,b,TRUE,ARGSRC_ENV)
#define parse_input_args_fmt(a,b)   parse_args(a,b,FALSE,ARGSRC_FORMAT)
#define parse_output_args_fmt(a,b)  parse_args(a,b,TRUE,ARGSRC_FORMAT)

static void parse_args(format_module *fm,char *argstr,bool is_output,int argsrc)
{
  char *argdup,*token,*seps = " \t",*extstr = "ext=",*argsrcstr;
  child_args *ca;

  if (NULL == argstr)
    return;

  argdup = strdup(argstr);
  if (NULL == argdup)
    st_error("argument string duplication failed");

  switch (argsrc) {
    case ARGSRC_ENV:
      argsrcstr = "environment";
      break;
    case ARGSRC_CMDLINE:
      argsrcstr = "command line";
      break;
    case ARGSRC_FORMAT:
    default:
      argsrcstr = PACKAGE;
      break;
  }

  st_debug1("parsing [%s] %s argument string originating from %s: [%s]",fm->name,(is_output)?"encoder":"decoder",argsrcstr,argstr);

  ca = (is_output) ? &fm->output_args_template : &fm->input_args_template;

  /* read first token */
  token = strtok(argdup,seps);

  /* skip certain arguments based on the source of the string we're parsing.
     format default = "arg1 arg2 ... argN"
     environment    = "prog arg1 arg2 ... argN"
     command line   = "format prog arg1 arg2 ... argN"
   */
  switch (argsrc) {
    case ARGSRC_CMDLINE:
      /* discard format name */
      if (NULL == token)
        return;

      token = strtok(NULL,seps);

      /* fall through */

    case ARGSRC_ENV:
      /* check for program name ("prog" or "ext=abc prog") */
      if (NULL == token)
        return;

      if (is_output) {
        /* output format: check for "ext=abc" */
        if (strstr(token,extstr) == token) {
          fm->extension = token + strlen(extstr);
          st_debug1("changing %s output extension to: [%s]",fm->name,fm->extension);
          token = strtok(NULL,seps);
        }

        if (NULL == token)
          return;

        /* output format: this is the program name */
        fm->encoder = token;
      }
      else {
        /* input format: this is the program name */
        fm->decoder = token;
      }

      arg_replace(ca,0,token);

      token = strtok(NULL,seps);

      /* fall through */

    case ARGSRC_FORMAT:
    default:
      break;
  }

  /* check if there are any program arguments specified */
  if (NULL == token)
    return;

  /* if more arguments exist, they must be encoder/decoder args, so start modifying the existing arg list */
  arg_reset(ca);
  arg_add(ca,(is_output)?fm->encoder:fm->decoder);

  while (token) {
    arg_add(ca,token);
    token = strtok(NULL,seps);
  }
}

static void rename_by_extension(child_args *outargs,char *filename,char *outfilename,char *input_extension,char *output_extension)
/* function to properly name an output file based on the input filename, prefix/postfixes, and extensions */
{
  char dirname[FILENAME_SIZE],
       newbasename[FILENAME_SIZE],
       outdirname[FILENAME_SIZE],
       custprefix[FILENAME_SIZE],
       custpostfix[FILENAME_SIZE],
       tmp[FILENAME_SIZE],
       *base,*ext,*outdir,*p,*extension;
  int i;

  /* get user-specified prefix/postfix in %f filename, if any */

  strcpy(custprefix,"");
  strcpy(custpostfix,"");

  if (outargs) {
    p = NULL;
    for (i=0;i<outargs->num_args;i++) {
      if (NULL == outargs->args[i])
        continue;
      if ((p = strstr(outargs->args[i],FILENAME_PLACEHOLDER)))
        break;
    }

    if (p) {
      /* get postfix (everything after %f) */
      strcpy(custpostfix,p+strlen(FILENAME_PLACEHOLDER));

      /* get prefix (everything before %f) */
      strcat(custprefix,outargs->args[i]);
      *(strstr(custprefix,FILENAME_PLACEHOLDER)) = 0;
    }
  }

  /* next, copy directory name, if any */
  strcpy(dirname,filename);
  base = basename(dirname);
  if (base)
    *base = 0;

  /* now get base filename as well as its extension */
  base = basename(filename);
  strcpy(newbasename,base);
  ext = extname(newbasename);

  /* if input filename's extension matches the input format's default extension, then swap it with the
     output format's extension.  otherwise, append the output format's extension to whatever's there */
  if (ext && input_extension && !strcmp(ext,input_extension))
    *(ext-1) = 0;

  /* set output directory */
  outdir = dirname;
  if (strcmp(st_ops.output_directory,""))
    outdir = st_ops.output_directory;

  strcpy(outdirname,outdir);

#if 0
  /* leave this out for now... we may be getting too fancy.  let the underlying OS handle it.
   * the repeated '/' case below is more straightforward, and harder to screw up, so that stays in for now.
   */

  /* remove all occurrences of '/./' - it's redundant */
  p = outdirname;
  while (*p) {
    if (0 == *(p+1))
      break;
    while ((*(p+2)) && (PATHSEPCHAR == *p) && ('.' == *(p+1)) && (PATHSEPCHAR == *(p+2))) {
      strcpy(p,p+2);
    }
    p++;
  }
#endif

  /* remove duplicate '/' (e.g. "/path///to/output///dir//" becomes "/path/to/output/dir/") */
  p = outdirname;
  while (*p) {
    while ((*(p+1)) && (PATHSEPCHAR == *p) && (PATHSEPCHAR == *(p+1))) {
      strcpy(p+1,p+2);
    }
    p++;
  }

  st_snprintf(tmp,FILENAME_SIZE,".%c",PATHSEPCHAR);

  if (!strcmp(outdirname,".") || !strcmp(outdirname,tmp))
    strcpy(outdirname,"");

  st_snprintf(tmp,FILENAME_SIZE,"%c",PATHSEPCHAR);

  if (strcmp(outdirname,"") && PATHSEPCHAR != outdirname[strlen(outdirname)-1])
    strcat(outdirname,tmp);

  extension = (output_extension) ? output_extension : "";

  /* now build full output filename */
  st_snprintf(outfilename,FILENAME_SIZE,"%s%s%s%s%s%s.%s",outdirname,custprefix,st_ops.output_prefix,newbasename,st_ops.output_postfix,custpostfix,extension);
}

static format_module *default_output_format()
{
  int i;

  for (i=0;st_formats[i];i++) {
    if (st_formats[i]->supports_output) {
      return st_formats[i];
    }
  }

  st_error("no output formats found");

  return NULL;
}

static int is_numeric(unsigned char *buf)
{
  unsigned char *p = buf;
  wlong bytes;

  if (0 == strlen((const char *)buf))
    return -1;

  while (*p) {
    if (!isdigit(*p))
      return -1;
    p++;
  }

  bytes =
#ifdef HAVE_ATOL
      (wlong)atol((const char *)buf);
#else
      (wlong)atoi((const char *)buf);
#endif

  return bytes;
}

static wlong is_m_ss(unsigned char *buf,wave_info *info)
{
  unsigned char *colon,*p;
  int len,min,sec;
  wlong bytes;

  len = strlen((const char *)buf);

  if (len < 4)
    return -1;

  colon = buf + len - 3;

  if (':' != *colon)
    return -1;

  p = buf;
  while (*p) {
    if (p != colon) {
      if (!isdigit(*p))
        return -1;
    }
    p++;
  }

  *colon = 0;

  min = atoi((const char *)buf);
  sec = atoi((const char *)(colon+1));

  if (sec >= 60)
    st_error("invalid value for seconds: [%d]",sec);

  bytes = (wlong)(min * info->rate * 60) +
          (wlong)(sec * info->rate);

  return bytes;
}

static wlong is_m_ss_ff(unsigned char *buf,wave_info *info)
{
  unsigned char *colon,*dot,*p;
  int len,min,sec,frames;
  wlong bytes;

  len = strlen((const char *)buf);

  if (len < 7)
    return -1;

  colon = buf + len - 6;
  dot = buf + len - 3;

  if (':' != *colon || '.' != *dot)
    return -1;

  p = buf;
  while (*p) {
    if (p != colon && p != dot) {
      if (!isdigit(*p))
        return -1;
    }
    p++;
  }

  *colon = 0;
  *dot = 0;

  if (PROB_NOT_CD(info))
    st_error("m:ss.ff format can only be used with CD-quality files");

  min = atoi((const char *)buf);
  sec = atoi((const char *)(colon+1));
  frames = atoi((const char *)(dot+1));

  if (sec >= 60)
    st_error("invalid value for seconds: [%d]",sec);

  if (frames >= 75)
    st_error("invalid value for frames: [%d]",frames);

  bytes = (wlong)(min * CD_RATE * 60) +
          (wlong)(sec * CD_RATE) +
          (wlong)(frames * CD_BLOCK_SIZE);

  return bytes;
}

static wlong is_m_ss_nnn(unsigned char *buf,wave_info *info)
{
  unsigned char *colon,*dot,*p;
  int len,min,sec,ms,nearest_byte,nearest_frame;
  wlong bytes;

  len = strlen((const char *)buf);

  if (len < 8)
    return -1;

  colon = buf + len - 7;
  dot = buf + len - 4;

  if (':' != *colon || '.' != *dot)
    return -1;

  p = buf;
  while (*p) {
    if (p != colon && p != dot) {
      if (!isdigit(*p))
        return -1;
    }
    p++;
  }

  *colon = 0;
  *dot = 0;

  min = atoi((const char *)buf);
  sec = atoi((const char *)(colon+1));
  ms = atoi((const char *)(dot+1));

  if (sec >= 60)
    st_error("invalid value for seconds: [%d]",sec);

  nearest_byte = (int)((((double)ms * (double)info->rate) / 1000.0) + 0.5);

  bytes = (wlong)(min * info->rate * 60) +
          (wlong)(sec * info->rate);

  if (PROB_NOT_CD(info)) {
    bytes += nearest_byte;
  }
  else {
    nearest_frame = (int)((double)ms * .075 + 0.5) * CD_BLOCK_SIZE;
    if (0 == nearest_frame && 0 == bytes) {
      st_warning("closest sector boundary to %d:%02d.%03d is the beginning of the file -- rounding up to first sector boundary",min,sec,ms);
      nearest_frame = CD_BLOCK_SIZE;
    }
    if (nearest_frame != nearest_byte) {
      st_warning("rounding %d:%02d.%03d (offset: %lu) to nearest sector boundary (offset: %lu)",
              min,sec,ms,bytes+(wlong)nearest_byte,bytes+(wlong)nearest_frame);
      bytes += (wlong)nearest_frame;
    }
  }

  return bytes;
}

/* Compare strings while treating digits characters numerically.
   Copyright (C) 1997, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jean-François Bignolles <bignolle@ecoledoc.ibp.fr>, 1997.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
           fractional parts, S_Z: idem but with leading Zeroes only */
#define S_N    0x0
#define S_I    0x4
#define S_F    0x8
#define S_Z    0xC

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define CMP    2
#define LEN    3


/* ISDIGIT differs from isdigit, as follows:
   - Its arg may be any int or unsigned int; it need not be an unsigned char.
   - It's guaranteed to evaluate its argument exactly once.
   - It's typically faster.
   Posix 1003.2-1992 section 2.5.2.1 page 50 lines 1556-1558 says that
   only '0' through '9' are digits.  Prefer ISDIGIT to isdigit unless
   it's important to use the locale's definition of `digit' even when the
   host does not conform to Posix.  */
#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)

#undef __strverscmp
#undef strverscmp

#ifndef weak_alias
# define __strverscmp strverscmp
#endif

/* Compare S1 and S2 as strings holding indices/version numbers,
   returning less than, equal to or greater than zero if S1 is less than,
   equal to or greater than S2 (for more info, see the texinfo doc).
*/

static int
__strverscmp (const char *s1, const char *s2)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;

  /* Symbol(s)    0       [1-9]   others  (padding)
     Transition   (10) 0  (01) d  (00) x  (11) -   */
  static const unsigned int next_state[] =
  {
      /* state    x    d    0    - */
      /* S_N */  S_N, S_I, S_Z, S_N,
      /* S_I */  S_N, S_I, S_I, S_I,
      /* S_F */  S_N, S_F, S_F, S_F,
      /* S_Z */  S_N, S_F, S_Z, S_Z
  };

  static const int result_type[] =
  {
      /* state   x/x  x/d  x/0  x/-  d/x  d/d  d/0  d/-
                 0/x  0/d  0/0  0/-  -/x  -/d  -/0  -/- */

      /* S_N */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_I */  CMP, -1,  -1,  CMP,  1,  LEN, LEN, CMP,
                  1,  LEN, LEN, CMP, CMP, CMP, CMP, CMP,
      /* S_F */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_Z */  CMP,  1,   1,  CMP, -1,  CMP, CMP, CMP,
                 -1,  CMP, CMP, CMP
  };

  if (p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;
  /* Hint: '0' is a digit too.  */
  state = S_N | ((c1 == '0') + (ISDIGIT (c1) != 0));

  while ((diff = c1 - c2) == 0 && c1 != '\0')
    {
      state = next_state[state];
      c1 = *p1++;
      c2 = *p2++;
      state |= (c1 == '0') + (ISDIGIT (c1) != 0);
    }

  state = result_type[state << 2 | ((c2 == '0') + (ISDIGIT (c2) != 0))];

  switch (state)
    {
    case CMP:
      return diff;

    case LEN:
      while (ISDIGIT (*p1++))
	if (!ISDIGIT (*p2++))
	  return 1;

      return ISDIGIT (*p2) ? -1 : diff;

    default:
      return state;
    }
}
#ifdef weak_alias
static weak_alias (__strverscmp, strverscmp)
#endif

static int compare_version(const wave_info **w1,const wave_info **w2)
{
  return strverscmp(w1[0]->filename,w2[0]->filename);
}

static int compare_ascii(const wave_info **w1,const wave_info **w2)
{
  return strcmp(w1[0]->filename,w2[0]->filename);
}

static void ascii_sort_files(wave_info **filenames, int numfiles)
{
  int (*cmpfunc) ();

  cmpfunc = compare_ascii;
  qsort(filenames,numfiles,sizeof(wave_info *),cmpfunc);
}

static void version_sort_files(wave_info **filenames,int numfiles)
{
  int (*cmpfunc) ();

  cmpfunc = compare_version;
  qsort(filenames,numfiles,sizeof(wave_info *),cmpfunc);
}

/* public functions */

FILE *open_input_stream_fmt(format_module *fmt,char *infile,proc_info *pinfo)
{
  if (fmt && fmt->supports_input) {
    /* if this format defines its own input function, run it */
    if (fmt->input_func)
      return fmt->input_func(infile,pinfo);

    return launch_input(fmt,infile,pinfo);
  }

  return NULL;
}

bool open_input_stream(wave_info *info)
/* opens an input stream, and if it contains an ID3v2 tag, skips past it */
{
  unsigned long bytes_to_read,tag_size;
  unsigned char tmp[BUF_SIZE];

  if (info->file_has_id3v2_tag) {
    if (info->input_format->decoder)
      st_debug1("decoder [%s] might fail to process ID3v2 tag detected in file: [%s]",info->input_format->decoder,info->filename);
    else
      st_debug1("discarding ID3v2 tag detected in file: [%s]",info->filename);
  }

  if (NULL == (info->input = open_input_stream_fmt(info->input_format,info->filename,&info->input_proc))) {
    st_warning("could not open file for streaming input: [%s]",info->filename);
    return FALSE;
  }

  /* check for ID3v2 tag on input stream */
  if ((tag_size = check_for_id3v2_tag(info->input))) {
    if (!info->stream_has_id3v2_tag) {
      if (info->input_format->decoder)
        st_debug1("discarding ID3v2 tag detected in input stream generated by decoder [%s] from file: [%s]",info->input_format->decoder,info->filename);
      else
        st_debug1("discarding ID3v2 tag detected in input stream from file: [%s]",info->filename);
      info->stream_has_id3v2_tag = TRUE;
    }

    if (0 == info->id3v2_tag_size)
      info->id3v2_tag_size = (wlong)(tag_size + sizeof(id3v2_header));

    st_debug1("discarding %lu-byte ID3v2 tag in input stream generated by decoder [%s] from file: [%s]",
      info->id3v2_tag_size,info->filename,info->input_format->decoder);

    while (tag_size > 0) {
      bytes_to_read = min(tag_size,BUF_SIZE);

      if (bytes_to_read != read_n_bytes(info->input,tmp,bytes_to_read,NULL)) {
        if (info->input_format->decoder)
          st_warning("error while discarding ID3v2 tag in input stream generated by decoder [%s] from file: [%s]",info->input_format->decoder,info->filename);
        else
          st_warning("error while discarding ID3v2 tag in input stream from file: [%s]",info->filename);
        close_input_stream(info);
        return FALSE;
      }

      tag_size -= bytes_to_read;
    }
  }
  else {
    close_input_stream(info);

    if (NULL == (info->input = open_input_stream_fmt(info->input_format,info->filename,&info->input_proc))) {
      st_warning("could not reopen file for streaming input: [%s]",info->filename);
      return FALSE;
    }
  }

  return TRUE;
}

void remove_file(char *filename)
{
  struct stat sz;
  int retval;

  if (st_ops.output_format && !st_ops.output_format->remove_output_file) {
    return;
  }

  if (stat(filename,&sz)) {
    st_debug1("tried to remove nonexistent file: [%s]",filename);
    return;
  }

  if ((retval = unlink(filename))) {
    st_warning("error while trying to remove partially-written output file: [%s]",filename);
    return;
  }

  st_debug1("successfully removed file: [%s]",filename);
}

bool files_are_identical(char *file1,char *file2)
{
  struct stat sz1,sz2;

  if (stat(file1,&sz1) || stat(file2,&sz2))
    return FALSE;

  if (!S_ISREG(sz1.st_mode) || !S_ISREG(sz2.st_mode))
    return FALSE;

  if (sz1.st_dev != sz2.st_dev)
    return FALSE;

  if (sz1.st_ino != sz2.st_ino)
    return FALSE;

  /* more checks, because windows inodes are always zero, which creates false positives */
  if (sz1.st_size != sz2.st_size)
    return FALSE;

  if (sz1.st_mtime != sz2.st_mtime)
    return FALSE;

  return TRUE;
}

bool odd_sized_data_chunk_is_null_padded(wave_info *info)
/* function to determine whether odd-sized data chunks are NULL-padded to an even length */
{
  FILE *devnull;
  unsigned char nullpad[BUF_SIZE];

  if (!PROB_ODD_SIZED_DATA(info))
    return TRUE;

  /* with no extra RIFF chunks, we can tell from the extra RIFF size whether it's padded */
  if (-1 == info->extra_riff_size)
    return FALSE;

  if (0 == info->extra_riff_size)
    return TRUE;

  /* it's odd-sized, and has extra RIFF chunks, so we'll have to make a pass through it to know for sure.
   * most modes don't need to do this, but ones that update chunk sizes on existing files need to know whether
   * it's padded in order to calculate the correct chunk size (currently this includes pad and strip modes).
   */

  if (NULL == (devnull = open_output(NULLDEVICE))) {
    return FALSE;
  }

  if (!open_input_stream(info)) {
    fclose(devnull);
    return FALSE;
  }

  st_debug1("scanning WAVE contents to determine whether odd-sized data chunk is padded with a NULL byte per RIFF specs");

  if (info->header_size + info->data_size != transfer_n_bytes(info->input,devnull,info->header_size + info->data_size,NULL)) {
    fclose(devnull);
    close_input_stream(info);
    return FALSE;
  }

  nullpad[0] = 1;

  if (0 == read_n_bytes(info->input,nullpad,1,NULL)) {
    fclose(devnull);
    close_input_stream(info);
    return FALSE;
  }

  fclose(devnull);
  close_input_stream(info);

  st_debug1("odd-sized data chunk is%s padded with a NULL byte in file: [%s]",(0==nullpad[0])?"":" not",info->filename);

  return (0 == nullpad[0]) ? TRUE : FALSE;
}

void st_snprintf(char *dest,int maxlen,char *formatstr, ...)
/* acts like snprintf, but makes 100% sure the string is NULL-terminated */
{
  va_list args;

  va_start(args,formatstr);

  st_vsnprintf(dest,maxlen,formatstr,args);

  dest[maxlen-1] = 0;

  va_end(args);
}

void length_to_str(wave_info *info)
/* converts length of file to a string in m:ss or m:ss.ff format */
{
  wlong newlength,rem1,rem2,frames,ms,h,m,s;
  double tmp;
  char ffnnn[8];

  /* get total seconds plus trailing portion of seconds */
  if (PROB_NOT_CD(info)) {
    newlength = (wlong)info->exact_length;

    tmp = info->exact_length - (double)((wlong)info->exact_length);
    ms = (wlong)((tmp * 1000.0) + 0.5);

    if (1000 == ms) {
      ms = 0;
      newlength++;
    }

    st_snprintf(ffnnn,8,"%03lu",ms);
  }
  else {
    newlength = info->length;

    rem1 = info->data_size % CD_RATE;
    rem2 = rem1 % CD_BLOCK_SIZE;

    frames = rem1 / CD_BLOCK_SIZE;
    if (rem2 >= (CD_BLOCK_SIZE / 2))
      frames++;

    if (frames == CD_BLOCKS_PER_SEC) {
      frames = 0;
      newlength++;
    }

    st_snprintf(ffnnn,8,"%02lu",frames);
  }

  /* calculate h:m:s */
  h = newlength / 3600;
  newlength -= h * 3600;
  m = newlength / 60;
  newlength -= m * 60;
  s = newlength;

  if (!st_priv.show_hmmss)
    m += h * 60;

  /* now build m_ss string, with h if necessary */
  if (st_priv.show_hmmss && h > 0)
    st_snprintf(info->m_ss,16,"%lu:%02lu:%02lu.%s",h,m,s,ffnnn);
  else
    st_snprintf(info->m_ss,16,"%lu:%02lu.%s",m,s,ffnnn);
}

int close_and_wait(FILE *fd,proc_info *pinfo,int child_type,format_module *fm)
{
  int retval;
#ifdef WIN32
  DWORD exitcode;
#else
  int gotpid,status;
  char tmp[BUF_SIZE],debuginfo[BUF_SIZE];
#endif

  retval = CLOSE_SUCCESS;

  /* never close stdin/stdout/stderr */
  if ((fd == stdin) || (fd == stdout) || (fd == stderr))
    return retval;

  if (fd) {
    fclose(fd);
    fd = NULL;
  }

  if (NO_CHILD_PID == pinfo->pid)
    return retval;

  /* the following seems to work fine under linux, for any decoder.  but we really only need it for certain ones. */
  if (CHILD_INPUT == child_type) {
    /*
     * kill input processes outright, since we're done with them.  this prevents programs from stalling us too long.
     * for example, 'la' doesn't exit when we close its stdout, but instead keeps decoding the whole file.  so each
     * time shntool opens an 'la' file, it will be decoded entirely at least four times before all is said and done,
     * even for simple things such as checking sizes in len mode.  a preemptive 'kill' here prevents this from
     * happening.  note that we can't do this for output child types, since we may unwittingly corrupt the output
     * file in the process -- for example, we would most certainly cause problems with .shn files by killing
     * 'shorten' while it is busy creating a seek table.
     *
     * this is now done on a per-format basis, with reasons given below:
     *
     * la   :  'la' decodes entire file no matter what, every time the input file is opened (see above).
     * aiff :  'sox' takes progressively longer as the input file becomes larger.
     * bonk :  'bonk' decodes entire file no matter what, every time the input file is opened (see above).
     * tta  :  'ttaenc' decodes entire file no matter what, every time the input file is opened (see above).
     */

    if (fm) {
      if (fm->kill_when_input_done) {
        st_debug2("preemptively killing [%s] input process %d to prevent possible delays",fm->decoder,pinfo->pid);
#ifdef WIN32
        TerminateProcess(pinfo->hProcess,0);
#else
        kill(pinfo->pid,SIGHUP);
#endif
      }
      else {
        st_debug2("waiting for [%s] input process %d to exit",fm->decoder,pinfo->pid);
      }
    }
    else {
      st_debug2("waiting for input process %d to exit",pinfo->pid);
    }
  }
  else {
    if (st_ops.output_format) {
      st_debug2("waiting for [%s] output process %d to exit",st_ops.output_format->encoder,pinfo->pid);
    }
    else {
      st_debug2("waiting for output process %d to exit",pinfo->pid);
    }
  }

#ifdef WIN32
  if ((exitcode = WaitForSingleObject(pinfo->hProcess,INFINITE)))
    st_debug2("WaitForSingleObject() return value: %d",exitcode);

  if (GetExitCodeProcess(pinfo->hProcess,&exitcode)) {
    st_debug2("process %d exit status: [%d]",pinfo->pid,exitcode);
  }
  else {
    st_debug2("process %d exit status could not be determined",pinfo->pid);
  }
#else
  gotpid = (int)waitpid((pid_t)pinfo->pid,&status,0);

  st_snprintf(debuginfo,BUF_SIZE,"process %d exit status: [%d",gotpid,WIFEXITED(status));
  if (WIFEXITED(status)) {
    st_snprintf(tmp,BUF_SIZE,"/%d",WEXITSTATUS(status));
    strcat(debuginfo,tmp);
  }
  st_snprintf(tmp,BUF_SIZE,"] [%d",WIFSIGNALED(status));
  strcat(debuginfo,tmp);
  if (WIFSIGNALED(status)) {
    st_snprintf(tmp,BUF_SIZE,"/%d",WTERMSIG(status));
    strcat(debuginfo,tmp);
  }
  st_snprintf(tmp,BUF_SIZE,"] [%d",WIFSTOPPED(status));
  strcat(debuginfo,tmp);
  if (WIFSTOPPED(status)) {
    st_snprintf(tmp,BUF_SIZE,"/%d",WSTOPSIG(status));
    strcat(debuginfo,tmp);
  }
  strcat(debuginfo,"]");

  st_debug2(debuginfo);
#endif

#ifdef WIN32
  if (0 != exitcode) {
#else
  if (WIFEXITED(status) && WEXITSTATUS(status)) {
#endif
    if (CHILD_OUTPUT == child_type) {
      st_warning("child encoder process %d had non-zero exit status %d",pinfo->pid,
#ifdef WIN32
                 exitcode);
#else
                 WEXITSTATUS(status));
#endif
      retval = CLOSE_CHILD_ERROR_OUTPUT;
    }
    else if (CHILD_INPUT == child_type) {
      retval = CLOSE_CHILD_ERROR_INPUT;
    }
  }

  return retval;
}

char *st_progname()
{
  return st_priv.fullprogname;
}

void alter_file_order(wave_info **filenames,int numfiles)
/* a simple menu system to change the order of the files as given on the command line */
{
  int i,current,a,b;
  char response[BUF_SIZE],
       *p,
       *args[3];
  wave_info *tmp;
  bool finished = FALSE;

  /* some modes read files/split points/etc. from stdin, so we can't ask.  force user to use -O option */
  if (!isatty(fileno(stdin)))
    st_error("standard input is not a terminal -- cannot invoke interactive file order editor");

  st_info("\nEntering file order editor.\n");

  while (!finished) {
    st_info("\n");
    st_info("File list:\n");
    st_info("\n");
    for (i=0;i<numfiles;i++)
      st_info("%2d: %s\n",i+1,filenames[i]->filename);
    st_info("\n");
    st_info("Commands:\n");
    st_info("\n");
    st_info("  [s]wap a b   (swaps positions a and b)\n");
    st_info("  [m]ove a b   (moves position a to position b, shifting everything in between)\n");
    st_info("  [b]egin a    (moves position a to the beginning of the list)\n");
    st_info("  [e]nd a      (moves position a to the end of the list)\n");
    st_info("  [n]atural    (sorts files using a natural sorting algorithm -- t1, t2, ... t10)\n");
    st_info("  [a]scii      (sorts files based on their ascii characters -- t1, t10, t2, ...)\n");
    st_info("  [d]one       (quits this editor, and continues with processing)\n");
    st_info("  [q]uit       (quits %s [you can also use Ctrl-C])\n",st_priv.progname);
    st_info("\n? ");
    fgets(response,BUF_SIZE-1,stdin);
    if (feof(stdin))
      strcpy(response,"done");
    trim(response);
    p = response;
    current = 0;
    args[0] = NULL;
    args[1] = NULL;
    args[2] = NULL;
    while (*p) {
      while (' ' == *p || '\t' == *p || '\n' == *p) {
        *p = 0;
        p++;
      }
      if (0 == *p)
        break;
      if (3 > current)
        args[current++] = p;
      while (*p && ' ' != *p && '\t' != *p && '\n' != *p)
        p++;
    }
    if (NULL == args[0])
      args[0] = "";

    st_info("\n");

    switch (args[0][0]) {
      case 's':
      case 'S':
        if (NULL == args[2])
          st_info("Not enough positions supplied to the 'swap' command\n");
        else {
          a = atoi(args[1]);
          b = atoi(args[2]);
          if ((a<1 || a>numfiles) || (b<1 || b>numfiles))
            st_info("One or more of your entries are out of range.  Try again.\n");
          else {
            st_info("Swapping positions %d and %d...\n",a,b);
            tmp = filenames[a-1];
            filenames[a-1] = filenames[b-1];
            filenames[b-1] = tmp;
          }
        }
        break;
      case 'm':
      case 'M':
        if (NULL == args[2])
          st_info("Not enough positions supplied to the 'move' command\n");
        else {
          a = atoi(args[1]);
          b = atoi(args[2]);
          if ((a<1 || a>numfiles) || (b<1 || b>numfiles))
            st_info("One or more of your entries are out of range.  Try again.\n");
          else {
            st_info("Moving position %d to position %d...\n",a,b);
            tmp = filenames[a-1];
            if (a < b) {
              for (i=a-1;i<b-1;i++)
                filenames[i] = filenames[i+1];
            }
            else if (a > b) {
              for (i=a-1;i>b-1;i--)
                filenames[i] = filenames[i-1];
            }
            filenames[b-1] = tmp;
          }
        }
        break;
      case 'b':
      case 'B':
        if (NULL == args[1])
          st_info("Missing a position for the 'begin' command.\n");
        else {
          a = atoi(args[1]);
          if ((a<1 || a>numfiles))
            st_info("Your entry is out of range.  Try again.\n");
          else {
            st_info("Moving position %d to the beginning of the list\n",a);
            tmp = filenames[a-1];
            for (i=a-1;i>0;i--)
              filenames[i] = filenames[i-1];
            filenames[0] = tmp;
          }
        }
        break;
      case 'e':
      case 'E':
        if (NULL == args[1])
          st_info("Missing a position for the 'end' command.\n");
        else {
          a = atoi(args[1]);
          if ((a<1 || a>numfiles))
            st_info("Your entry is out of range.  Try again.\n");
          else {
            st_info("Moving position %d to the end of the list...\n",a);
            tmp = filenames[a-1];
            for (i=a-1;i<numfiles-1;i++)
              filenames[i] = filenames[i+1];
            filenames[numfiles-1] = tmp;
          }
        }
        break;
      case 'n':
      case 'N':
        version_sort_files(filenames,numfiles);
        break;
      case 'a':
      case 'A':
        ascii_sort_files(filenames,numfiles);
        break;
      case 'd':
      case 'D':
        st_info("Exiting file order editor.\n");
        finished = TRUE;
        break;
      case 'q':
      case 'Q':
        st_info("Quitting %s.\n",st_priv.progname);
        exit(ST_EXIT_QUIT);
        break;
      default:
        st_info("Unrecognized command: %s\n",args[0]);
        break;
    }
  }

  st_info("\n");
}

wlong smrt_parse(unsigned char *data,wave_info *info)
/* currently not so smart, so I gave it the Homer Simpson spelling :) */
{
  wlong bytes;
  unsigned char tmp[BUF_SIZE];

  strcpy((char *)tmp,(const char *)data);

  /* check for all digits */
  if (-1 != (bytes = is_numeric(tmp)))
    return bytes;

  /* check for m:ss */
  if (-1 != (bytes = is_m_ss(tmp,info)))
    return bytes;

  /* check for m:ss.ff */
  if (-1 != (bytes = is_m_ss_ff(tmp,info)))
    return bytes;

  /* check for m:ss.nnn */
  if (-1 != (bytes = is_m_ss_nnn(tmp,info)))
    return bytes;

  /* it was not in any of these formats */

  st_error("value not in bytes, m:ss, m:ss.ff, or m:ss.nnn format: [%s]",data);

  return -1;
}

int st_getopt(int argc,char **argv,char *mode_opts)
{
  char ops[BUF_SIZE],*p,c[2],*global_opts;
  int opt;
  format_module *input_format = NULL;

  global_opts = (st_priv.mode->creates_files) ? GLOBAL_OPTS GLOBAL_OPTS_OUTPUT : GLOBAL_OPTS;

  /* make sure the calling mode isn't using an option reserved for shntool global use */
  c[1] = 0;
  for (p=global_opts;*p;p++) {
    if (':' == *p)
      continue;
    c[0] = *p;
    if (strstr(mode_opts,c))
      st_error("mode attempted to use global option: [-%s]",c);
  }

  if (st_priv.mode->creates_files && (NULL == st_ops.output_format))
    st_ops.output_format = default_output_format();

  st_snprintf(ops,BUF_SIZE,"%s%s",global_opts,mode_opts);

  opt = getopt(argc,argv,ops);

  if (-1 == opt)
    return opt;

  /* handle global options before mode options */
  switch (opt) {
    case 'D':
      st_priv.debug_level++;
      break;
    case 'F':
      if (NULL == optarg)
        st_help("missing input file filename");
      st_input.type = INPUT_FILE;
      st_input.filename_source = optarg;
      break;
    case 'H':
      st_priv.show_hmmss = TRUE;
      break;
    case 'P':
      if (NULL == optarg)
        st_help("missing progress indicator type");
      if (!strcmp(optarg,"pct"))
        st_priv.progress_type = PROGRESS_PERCENT;
      else if (!strcmp(optarg,"dot"))
        st_priv.progress_type = PROGRESS_DOT;
      else if (!strcmp(optarg,"spin"))
        st_priv.progress_type = PROGRESS_SPIN;
      else if (!strcmp(optarg,"face"))
        st_priv.progress_type = PROGRESS_FACE;
      else if (!strcmp(optarg,"none"))
        st_priv.progress_type = PROGRESS_NONE;
      else
        st_help("invalid progress indicator type: [%s]",optarg);
      break;
    case '?':
    case 'h':
      if ('?' == opt)
        st_info("\n");
      if (st_priv.mode->run_help)
        st_priv.mode->run_help();
      st_global_usage();
      exit(('?' == opt) ? ST_EXIT_ERROR : ST_EXIT_SUCCESS);
      break;
    case 'i':
      if (NULL == optarg)
        st_help("missing input format");
      if (NULL == (input_format = find_format(optarg)))
        st_help("missing input format name");
      if (!input_format->supports_input)
        st_help("format does not support input: [%s]",input_format->name);
      parse_input_args_cmd(input_format,optarg);
      break;
    case 'q':
      st_priv.suppress_stderr = TRUE;
      break;
    case 'r':
      if (NULL == optarg)
        st_help("missing reorder type");
      if (!strcmp(optarg,"ask"))
        st_priv.reorder_type = ORDER_ASK;
      else if (!strcmp(optarg,"ascii"))
        st_priv.reorder_type = ORDER_ASCII;
      else if (!strcmp(optarg,"natural"))
        st_priv.reorder_type = ORDER_NATURAL;
      else if (!strcmp(optarg,"none"))
        st_priv.reorder_type = ORDER_AS_IS;
      else
        st_help("invalid reorder type: [%s]",optarg);
      break;
    case 'v':
      st_version();
      exit(ST_EXIT_SUCCESS);
      break;
    case 'w':
      st_priv.suppress_warnings = TRUE;
      break;
  }

  if (st_priv.mode->creates_files) {
    switch (opt) {
      case 'O':
        if (NULL == optarg)
          st_help("missing overwrite action");
        if (!strcmp(optarg,"ask"))
          st_priv.clobber_action = CLOBBER_ACTION_ASK;
        else if (!strcmp(optarg,"always"))
          st_priv.clobber_action = CLOBBER_ACTION_ALWAYS;
        else if (!strcmp(optarg,"never"))
          st_priv.clobber_action = CLOBBER_ACTION_NEVER;
        else
          st_help("invalid overwrite action: [%s]",optarg);
        break;
      case 'a':
        if (NULL == optarg)
          st_help("missing filename prefix");
        st_ops.output_prefix = optarg;
        break;
      case 'd':
        if (NULL == optarg)
          st_help("missing output directory");
        st_ops.output_directory = optarg;
        break;
      case 'o':
        if (NULL == optarg)
          st_help("missing output format");
        if (NULL == (st_ops.output_format = find_format(optarg)))
          st_help("missing output format name");
        if (!st_ops.output_format->supports_output)
          st_help("format does not support output: [%s]",st_ops.output_format->name);
        parse_output_args_cmd(st_ops.output_format,optarg);
        break;
      case 'z':
        if (NULL == optarg)
          st_help("missing filename postfix");
        st_ops.output_postfix = optarg;
        break;
    }
  }

  return opt;
}

void discard_header(wave_info *info)
{
  unsigned char *header;

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char))))
    st_error("could not allocate %d bytes for WAVE header",info->header_size);

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size)
    st_error("error while discarding %d-byte header from file: [%s]",info->header_size,info->filename);

  st_free(header);
}

void create_output_filename(char *infile,char *inext,char *outfile)
{
  strcpy(outfile,"");

  if (st_ops.output_format) {
    if (st_ops.output_format->create_output_filename) {
      st_ops.output_format->create_output_filename(outfile);
      return;
    }

    rename_by_extension(
      (st_ops.output_format->encoder) ? &st_ops.output_format->output_args_template : NULL,
      infile,outfile,inext,st_ops.output_format->extension);
  }
}

FILE *open_output_stream(char *outfile,proc_info *pinfo)
{
  pinfo->pid = NO_CHILD_PID;

  if (st_ops.output_format && st_ops.output_format->supports_output) {
    /* if this format defines its own output function, run it */
    if (st_ops.output_format->output_func)
      return st_ops.output_format->output_func(outfile,pinfo);

    return launch_output(st_ops.output_format,outfile,pinfo);
  }

  return NULL;
}

void st_global_usage()
{
  st_info("Global options:\n");
  st_info("\n");
  st_info("  -D      print debugging information (each one increases debugging level)\n");
  st_info("  -F file get input filenames from file, instead of command line or terminal\n");
  st_info("  -H      print times in h:mm:ss.{ff,nnn} format, instead of m:ss.{ff,nnn}\n");
  if (st_priv.mode->creates_files) {
    st_info("  -O val  overwrite existing files?  val is: {[ask], always, never}\n");
  }
  st_info("  -P type progress indicator type.  type is: {[pct], dot, spin, face, none}\n");
  if (st_priv.mode->creates_files) {
    st_info("  -a str  prefix 'str' to base part of output filenames\n");
    st_info("  -d dir  specify output directory\n");
  }
  st_info("  -i fmt  specify input file format decoder and/or arguments.\n");
  st_info("          format is:  \"fmt decoder [arg1 ... argN (%s = filename)]\"\n",FILENAME_PLACEHOLDER);
  if (st_priv.mode->creates_files) {
    st_info("  -o fmt  specify output file format, extension, encoder and/or arguments.\n");
    st_info("          format is:  \"fmt [ext=abc] [encoder [arg1 ... argN (%s = filename)]]\"\n",FILENAME_PLACEHOLDER);
  }
  st_info("  -q      suppress non-critical output (quiet mode)\n");
  st_info("  -r val  reorder input files?  val is: {ask, ascii, [natural], none}\n");
  st_info("  -v      show version information\n");
  st_info("  -w      suppress warnings\n");
  if (st_priv.mode->creates_files) {
    st_info("  -z str  postfix 'str' to base part of output filenames\n");
  }
  st_info("  --      indicates that everything following it is a filename\n");
  st_info("\n");
}

void arg_init(format_module *fm)
{
  char *p,envname[BUF_SIZE],upper[BUF_SIZE];

  strcpy(upper,fm->name);

  /* convert format name to upper case */
  for (p=upper;*p;p++) {
    if (*p >= 'a' && *p <= 'z')
      *p = *p - 32;
  }

  if (fm->supports_input) {
    /* initialize with format defaults */
    parse_input_args_fmt(fm,fm->decoder_args);

    /* check environment for user-specified decoder */
    st_snprintf(envname,BUF_SIZE,"ST_%s_DEC",upper);
    parse_input_args_env(fm,scan_env(envname));
  }

  if (fm->supports_output) {
    /* initialize with format defaults */
    parse_output_args_fmt(fm,fm->encoder_args);

    /* check environment for user-specified extension and/or encoder */
    st_snprintf(envname,BUF_SIZE,"ST_%s_ENC",upper);
    parse_output_args_env(fm,scan_env(envname));
  }
}

void arg_add(child_args *child_args,char *arg)
{
  if (child_args->num_args >= MAX_CHILD_ARGS)
    st_error("too many arguments specified -- limit is %d",MAX_CHILD_ARGS);

  child_args->args[child_args->num_args] = arg;
  child_args->num_args++;
}

void arg_reset(child_args *child_args)
{
  int i;

  child_args->num_args = 0;

  for (i=0;i<MAX_CHILD_ARGS;i++)
    child_args->args[i] = NULL;
}

void arg_replace(child_args *child_args,int pos,char *arg)
{
  child_args->args[pos] = arg;
}

void reorder_files(wave_info **files,int numfiles)
{
  switch (st_priv.reorder_type) {
    case ORDER_ASK:
      alter_file_order(files,numfiles);
      break;
    case ORDER_NATURAL:
      version_sort_files(files,numfiles);
      break;
    case ORDER_ASCII:
      ascii_sort_files(files,numfiles);
      break;
    case ORDER_AS_IS:
    default:
      break;
  }
}

static void prog_print_data(progress_info *proginfo)
{
  if (proginfo->prefix) {
    st_info("%s ",proginfo->prefix);
  }

  if (proginfo->filename1) {
    st_info("[%s] ",proginfo->filename1);
    if (proginfo->filedesc1) {
      st_info("(%s) ",proginfo->filedesc1);
    }
  }

  if (proginfo->clause) {
    st_info("%s ",proginfo->clause);
  }

  if (proginfo->filename2) {
    st_info("[%s] ",proginfo->filename2);
    if (proginfo->filedesc2) {
      st_info("(%s) ",proginfo->filedesc2);
    }
  }

  st_info(": ");
}

static void prog_init(progress_info *proginfo)
{
  if (!proginfo->initialized) {
    proginfo->bytes_written = 0;
    proginfo->dot_step = -1;
    proginfo->last_percent = -1;
    proginfo->progress_shown = FALSE;
    prog_print_data(proginfo);
    proginfo->initialized = TRUE;
    st_priv.screen_dirty = FALSE;
  }

  if (st_priv.screen_dirty) {
    proginfo->last_percent = -1;
    proginfo->progress_shown = FALSE;
    prog_print_data(proginfo);
    st_priv.screen_dirty = FALSE;
  }
}

static void prog_uninit(progress_info *proginfo)
{
  proginfo->initialized = FALSE;
  proginfo->progress_shown = FALSE;
  proginfo->bytes_written = 0;
  proginfo->bytes_total = 0;
  proginfo->dot_step = 0;
  proginfo->percent = 0;
  proginfo->last_percent = 0;
}

static void prog_erase_chars(progress_info *proginfo,int erasecnt)
{
  int i;

  if (!proginfo->progress_shown) {
    proginfo->progress_shown = TRUE;
    return;
  }

  for (i=0;i<erasecnt;i++)
    st_info("\b");
}

static void prog_show_pct(progress_info *proginfo)
{
  prog_erase_chars(proginfo,5);

  st_info("%3d%% ",proginfo->percent);
}

static void prog_show_dot(progress_info *proginfo)
{
  if (-1 == proginfo->dot_step)
    proginfo->dot_step = 0;

  while ((proginfo->dot_step + 10) <= proginfo->percent) {
    st_info(".");
    proginfo->dot_step += 10;
  }
}

static void prog_show_spin(progress_info *proginfo)
{
  char spin[4] = { '|', '/', '-', '\\' };

  while ((proginfo->dot_step + 1) <= proginfo->percent) {
    proginfo->dot_step++;
    prog_erase_chars(proginfo,2);
    st_info("%c ",spin[proginfo->dot_step % 4]);
  }
}

static void prog_show_face(progress_info *proginfo)
{
  char *faces[6] = { ":-(", ":-/", ":-\\", ":-|", ":-)", ":-D" };
  bool show_face = FALSE;

  if (-1 == proginfo->dot_step)
    proginfo->dot_step = -20;

  while ((proginfo->dot_step + 20) <= proginfo->percent) {
    proginfo->dot_step += 20;
    show_face = TRUE;
  }

  if (!show_face)
    return;

  prog_erase_chars(proginfo,4);

  st_info("%s ",faces[proginfo->dot_step/20]);
}

static void prog_finish(char *status,progress_info *proginfo)
{
  if (PROGRESS_DOT == st_priv.progress_type)
    st_info(" ");

  st_info("%s\n",status);

  prog_uninit(proginfo);
}

void prog_update(progress_info *proginfo)
{
  prog_init(proginfo);

  if (proginfo->bytes_total <= 0)
    proginfo->percent = 0;
  else
    proginfo->percent = (int)(100.0 * ((double)proginfo->bytes_written)/((double)proginfo->bytes_total));

  proginfo->percent = min(max(proginfo->percent,0),100);

  if (proginfo->percent <= proginfo->last_percent)
    return;

  switch (st_priv.progress_type) {
    case PROGRESS_PERCENT:
      prog_show_pct(proginfo);
      break;
    case PROGRESS_DOT:
      prog_show_dot(proginfo);
      break;
    case PROGRESS_SPIN:
      prog_show_spin(proginfo);
      break;
    case PROGRESS_FACE:
      prog_show_face(proginfo);
      break;
    case PROGRESS_NONE:
    default:
      break;
  }

  proginfo->last_percent = proginfo->percent;
}

void prog_success(progress_info *proginfo)
{
  proginfo->bytes_written = proginfo->bytes_total;

  prog_update(proginfo);

  prog_finish("OK",proginfo);
}

void prog_error(progress_info *proginfo)
{
  prog_update(proginfo);

  prog_finish("ERROR",proginfo);
}

void input_init(int argn,int argc,char **argv)
{
  if (INPUT_FILE != st_input.type && INPUT_INTERNAL != st_input.type) {
    if (argn >= argc) {
      st_input.type = INPUT_STDIN;
    }
    else {
      st_input.type = INPUT_CMDLINE;
    }
  }

  switch (st_input.type) {
    case INPUT_CMDLINE:
      st_debug1("reading input filenames from command line");
      st_input.argn = argn;
      st_input.argc = argc;
      st_input.argv = argv;
      break;

    case INPUT_STDIN:
      st_debug1("reading input filenames from stdin");
      if (isatty(fileno(stdin)))
        st_info("enter input filename(s):\n");
      st_input.fd = stdin;
      break;

    case INPUT_FILE:
      st_debug1("reading input filenames from file: [%s]",st_input.filename_source);
      if (NULL == (st_input.fd = fopen(st_input.filename_source,"rb"))) {
        st_error("could not open input filename file: [%s]",st_input.filename_source);
      }
      break;

    case INPUT_INTERNAL:
      st_input.filecur = 0;
      break;

    default:
      break;
  }
}

char *input_get_filename()
{
  static char internal_filename[FILENAME_SIZE];
  char *filename = NULL;

  switch (st_input.type) {
    case INPUT_CMDLINE:
      if (st_input.argn < st_input.argc) {
        filename = st_input.argv[st_input.argn];
        st_input.argn++;
      }
      break;

    case INPUT_STDIN:
    case INPUT_FILE:
      fgets(internal_filename,FILENAME_SIZE-1,st_input.fd);
      if (!feof(st_input.fd)) {
        trim(internal_filename);
        filename = internal_filename;
      }
      else {
        if (INPUT_FILE == st_input.type) {
          fclose(st_input.fd);
        }
      }
      break;

    case INPUT_INTERNAL:
      if (st_input.filecur < st_input.filemax) {
        st_debug1("returning file %d: [%s]",st_input.filecur,st_input.filenames[st_input.filecur]);
        filename = st_input.filenames[st_input.filecur];
        st_input.filecur++;
      }
      break;

    default:
      break;
  }

  return filename;
}

void input_read_all_files()
{
  char *filename;

  st_input.filemax = 0;

  while ((filename = input_get_filename())) {
    st_input.filenames[st_input.filemax] = strdup(filename);
    st_input.filemax++;
    if (st_input.filemax >= MAX_FILENAMES)
      st_error("exceeded maximum number of filenames: [%s]",MAX_FILENAMES);
  }

  st_input.type = INPUT_INTERNAL;
  st_input.filecur = 0;
}

int input_get_file_count()
{
  return st_input.filemax;
}
