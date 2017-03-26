/*  core_format.c - public functions for format modules
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "shntool.h"

CVSID("$Id: core_format.c,v 1.44 2009/03/11 17:18:01 jason Exp $")

/* private helper functions */

#define spawn_input(a,b,c,d)       spawn(a,b,c,d,CHILD_INPUT,NULL)
#define spawn_output(a,b,c,d)      spawn(a,b,c,d,CHILD_OUTPUT,NULL)
#define spawn_input_fd(a,b,c,d,e)  spawn(a,b,c,d,CHILD_INPUT,e)
#define spawn_output_fd(a,b,c,d,e) spawn(a,b,c,d,CHILD_OUTPUT,e)

static void get_quoted_arg_list(child_args *process_args,char *arg_list)
{
  int i;
  char tmp[BUF_SIZE];

  strcpy(arg_list,"");

  for (i=0;i<process_args->num_args;i++) {
    st_snprintf(tmp,BUF_SIZE,"%s\"%s\"",(0==i)?"":" ",process_args->args[i]);
    strcat(arg_list,tmp);
  }
}

static void spawn(child_args *process_args,FILE **readpipe,FILE **writepipe,proc_info *pinfo,int child_type,FILE *inputstream)
/* forks off a process running the command cmd, and sets up read/write
 * pipes for two-way communication with that process
 */
{
  char quoted_args[BUF_SIZE];
#ifdef WIN32
  /* the bulk of the following code was taken from Microsoft Knowledge Base
   * Article ID 190351 (Revision 7.1) -- "How to spawn console processes with
   * redirected standard handles".  URL: <http://support.microsoft.com/kb/190351>
   */
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  SECURITY_ATTRIBUTES sa;
  HANDLE hOutputReadTmp,hOutputRead,hOutputWrite;
  HANDLE hInputWriteTmp,hInputRead,hInputWrite;
  HANDLE hErrorWrite;

  /* set security attributes */
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  /* create the child stdout pipe */
  if (!CreatePipe(&hOutputReadTmp,&hOutputWrite,&sa,0)) {
    st_error("error while creating output pipe for two-way communication with child process");
  }

  /* create the child stdin pipe */
  if (!CreatePipe(&hInputRead,&hInputWriteTmp,&sa,0)) {
    st_error("error while creating input pipe for two-way communication with child process");
  }

  /* send child stderr to /dev/null */
  if (!(hErrorWrite = CreateFile(NULLDEVICE,GENERIC_WRITE,FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL))) {
    st_error("error while redirecting child stderr");
  }

  /* connect child stdin to parent output */
  if (!DuplicateHandle(GetCurrentProcess(),hOutputReadTmp,GetCurrentProcess(),&hOutputRead,0,FALSE,DUPLICATE_SAME_ACCESS)) {
    st_error("error while duplicating input pipe for two-way communication with child process");
  }

  /* connect child stdout to parent input */
  if (!DuplicateHandle(GetCurrentProcess(),hInputWriteTmp,GetCurrentProcess(),&hInputWrite,0,FALSE,DUPLICATE_SAME_ACCESS)) {
    st_error("error while duplicating error pipe for two-way communication with child process");
  }

  /* close unneeded handles */
  if (!CloseHandle(hOutputReadTmp) || !CloseHandle(hInputWriteTmp)) {
    st_error("error while closing handle(s)");
  }

  /* set up child process environment */
  ZeroMemory(&si,sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hOutputWrite;
  si.hStdInput  = hInputRead;
  si.hStdError  = hErrorWrite;
  si.wShowWindow = SW_HIDE;

  /* get quoted args */
  get_quoted_arg_list(process_args,quoted_args);

  if (!CreateProcess(NULL,quoted_args,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi)) {
    st_warning("error while launching helper program: [%s]",process_args->args[0]);
    *readpipe = NULL;
    *writepipe = NULL;
    pinfo->pid = NO_CHILD_PID;
    pinfo->hProcess = NULL;
    return;
  }

  /* grab the child pid */
  pinfo->pid = (int)pi.dwProcessId;
  pinfo->hProcess = pi.hProcess;

  if (!CloseHandle(pi.hThread)) {
    st_error("error closing thread handle");
  }

  if (!CloseHandle(hOutputWrite) || !CloseHandle(hInputRead) || !CloseHandle(hErrorWrite)) {
    st_error("error while closing handle(s)");
  }

  *readpipe = fdopen(_open_osfhandle((intptr_t)hOutputRead,0),"rb");
  *writepipe = fdopen(_open_osfhandle((intptr_t)hInputWrite,0),"wb");
#else
  int pipe1[2], pipe2[2];
  int i,nullfd,max_fds;

  /* create pipes for two-way communication */
  if ((pipe(pipe1) < 0) || (pipe(pipe2) < 0)) {
    perror("shntool: pipe");
    st_error("error while creating pipes for two-way communication with child process");
  }

  /* fork child process */
  switch ((pinfo->pid = fork())) {
    case -1:
      /* fork failed */
      perror("shntool: fork");
      st_error("error while forking child process");
      break;
    case 0:
      /* child */

      close(pipe1[1]);
      close(pipe2[0]);

      /* Read from parent on pipe1[0], write to parent on pipe2[1]. */

      if (inputstream) {
        dup2(fileno(inputstream),0);
        close(fileno(inputstream));
      }
      else {
        dup2(pipe1[0],0);
      }

      dup2(pipe2[1],1);

      close(pipe1[0]);
      close(pipe2[1]);

      /* make sure stderr is connected to /dev/null, or closed if that fails */
      close(2);
      nullfd = open(NULLDEVICE,O_WRONLY);
      if (nullfd > 0 && 2 != nullfd)
        dup2(nullfd,2);

      SETBINARY_FD(0);
      SETBINARY_FD(1);
      SETBINARY_FD(2);

#ifdef HAVE_SYSCONF
      max_fds = sysconf(_SC_OPEN_MAX);
#else
      max_fds = 1024;
#endif

      /* close all other file descriptors */
      for (i=3;i<max_fds;i++)
        close(i);

      if (execvp(process_args->args[0],process_args->args) < 0) {
        perror("shntool: execvp");
        st_error("error while launching helper program: [%s]",process_args->args[0]);
      }

      break;
    default:
      /* parent */
      close(pipe1[0]);
      close(pipe2[1]);

      /* Write to child on pipe1[1], read from child on pipe2[0]. */
      *readpipe = fdopen(pipe2[0],"rb");
      *writepipe = fdopen(pipe1[1],"wb");

      /* set characteristics of pipes */
      SETBINARY_IN(*readpipe);
      SETBINARY_OUT(*writepipe);

      break;
    }

  /* get quoted args */
  get_quoted_arg_list(process_args,quoted_args);
#endif

  st_debug2("spawned %s process with pid %d and command line: %s",(CHILD_INPUT == child_type)?"input":"output",pinfo->pid,quoted_args);
}

static void arg_build(child_args *result,child_args *template,char *filename)
{
  int i;

  arg_reset(result);

  for (i=0;i<template->num_args;i++) {
    if (NULL == template->args[i]) {
      if (0 == i) {
        st_error("while building argument list: encoder/decoder not defined or unspecified");
      }
      else {
        st_error("while building argument list: encoder/decoder argument %d not defined",i);
      }
    }
    if (strstr(template->args[i],FILENAME_PLACEHOLDER)) {
      arg_add(result,filename);
    }
    else {
      arg_add(result,template->args[i]);
    }
  }
}

static bool clobber_ask(char *filename)
{
  char response[BUF_SIZE];

  st_debug1("asking whether to clobber file: [%s]",filename);

  st_info("\n");

  /* some modes read files/split points/etc. from stdin, so we can't ask.  force user to use -O option */
  if (!isatty(fileno(stdin)) || feof(stdin))
    st_error("standard input is not a terminal -- cannot prompt for overwrite action.  use \"-O always\" or \"-O never\".");

  st_priv.screen_dirty = TRUE;

  while (1) {
    st_info("File already exists: [%s]\n",filename);
    st_info("Overwrite ([y]es, [n]o, [a]lways, ne[v]er, [r]ename)? ");
    fgets(response,BUF_SIZE-1,stdin);
    if (feof(stdin))
      st_info("\n");
    trim(response);

    switch (response[0]) {
      case 'y':
      case 'Y':
        return TRUE;
        break;
      case 'n':
      case 'N':
        return FALSE;
        break;
      case 'a':
      case 'A':
        st_priv.clobber_action = CLOBBER_ACTION_ALWAYS;
        return TRUE;
        break;
      case 'v':
      case 'V':
        st_priv.clobber_action = CLOBBER_ACTION_NEVER;
        return FALSE;
        break;
      case 'r':
      case 'R':
        strcpy(response,"");
        while (!strcmp(response,"")) {
          st_info("New name: ");
          fgets(response,BUF_SIZE-1,stdin);
          trim(response);
        }
        strcpy(filename,response);
        return clobber_check(filename);
        break;
      default:
        break;
    }
  }
}

static void verify_format_generic(format_module *fm,child_args *template,char *type)
{
  int i;
  bool found_filename;

  found_filename = FALSE;

  for (i=0;i<template->num_args;i++) {
    if (NULL == template->args[i]) {
      if (0 == i) {
        st_error("%s argument list does not specify the %s for format: [%s]",type,type,fm->name);
      }
      else {
        st_error("%s argument %d is undefined for format: [%s]",type,i,fm->name);
      }
    }
    if ((0 != i) && strstr(template->args[i],FILENAME_PLACEHOLDER)) {
      found_filename = TRUE;
    }
  }

  if (!found_filename)
    st_error("%s argument list does not contain filename placeholder [%s] for format: [%s]",type,FILENAME_PLACEHOLDER,fm->name);
}

static void verify_format_input(format_module *fm)
{
  if (!fm->decoder)
    st_error("format requires a decoder: [%s]",fm->name);

  verify_format_generic(fm,&fm->input_args_template,"decoder");
}

static void verify_format_output(format_module *fm)
{
  if (!fm->encoder)
    st_error("format requires an encoder: [%s]",fm->name);

  verify_format_generic(fm,&fm->output_args_template,"encoder");
}

/* public functions */

void tagcpy(unsigned char *dest,unsigned char *src)
/* simple copy of a tag from src to dest.  NOTE: src MUST be NULL-terminated,
 * and the trailing NULL byte is NOT copied.  This routine assumes that dest
 * is large enough to contain src.
 */
{
  int i;

  for (i=0;*(src+i);i++)
    *(dest+i) = *(src+i);
}

int tagcmp(unsigned char *got,unsigned char *expected)
/* compare got against expected, up to the length of expected */
{
  int i;

  for (i=0;*(expected+i);i++) {
    if (*(got+i) != *(expected+i))
      return i+1;
  }

  return 0;
}

bool check_for_magic(char *filename,char *magic,int offset)
{
  FILE *file;
  int magiclen, bytes, bytes_to_discard;
  unsigned char buf[XFER_SIZE];

  if (NULL == magic || !strcmp(magic,""))
    return FALSE;

  if (offset < 0)
    return FALSE;

  if (NULL == (file = open_input(filename)))
    return FALSE;

  magiclen = strlen(magic);

  bytes_to_discard = offset;

  /* skip to magic header */
  while (bytes_to_discard > 0) {
    bytes = min(bytes_to_discard,XFER_SIZE);
    if (bytes!= fread(buf,1,bytes,file)) {
      fclose(file);
      return FALSE;
    }
    bytes_to_discard -= bytes;
  }

  /* read magic header */
  if (magiclen != fread(buf,1,magiclen,file)) {
    fclose(file);
    return FALSE;
  }

  fclose(file);

  if (!tagcmp(buf,(unsigned char *)magic))
    return TRUE;

  return FALSE;
}

format_module *find_format(char *fmtname)
{
  int i;
  format_module *op = NULL;
  char *p,*seps = " \t",tmp[BUF_SIZE];

  if (NULL == fmtname)
    st_help("missing file format");

  strcpy(tmp,fmtname);
  p = strtok(tmp,seps);

  if (NULL == p)
    return NULL;

  for (i=0;st_formats[i];i++) {
    if (!strcmp(p,st_formats[i]->name)) {
      op = st_formats[i];
      break;
    }
  }

  if (NULL == op)
    st_help("invalid file format: [%s]",p);

  return op;
}

FILE *launch_input(format_module *fm,char *filename,proc_info *pinfo)
{
  FILE *input,*output,*f;
  bool file_has_id3v2_tag;

  verify_format_input(fm);

  if (fm->stdin_for_id3v2_kluge) {
    /* check for ID3v2 tag on input */
    f = open_input_internal(filename,&file_has_id3v2_tag,NULL);

    if (f && file_has_id3v2_tag) {
      st_debug1("input file has an ID3v2 tag -- attempting to trick helper program: [%s]",fm->decoder);
    }
    else {
      file_has_id3v2_tag = FALSE;
    }

    arg_build(&fm->input_args,&fm->input_args_template,(file_has_id3v2_tag)?fm->stdin_for_id3v2_kluge:filename);

    if (file_has_id3v2_tag) {
      spawn_input_fd(&fm->input_args,&input,&output,pinfo,f);
      fclose(f);
    }
    else {
      spawn_input(&fm->input_args,&input,&output,pinfo);
    }
  }
  else {
    arg_build(&fm->input_args,&fm->input_args_template,filename);

    spawn_input(&fm->input_args,&input,&output,pinfo);
  }

  if (output)
    fclose(output);

  return input;
}

FILE *launch_output(format_module *fm,char *filename,proc_info *pinfo)
{
  FILE *input,*output;

  verify_format_output(fm);

  if (!clobber_check(filename))
    return NULL;

  arg_build(&fm->output_args,&fm->output_args_template,filename);

  spawn_output(&fm->output_args,&input,&output,pinfo);

  if (input)
    fclose(input);

  return output;
}

bool clobber_check(char *filename)
/* function to check if a file is about to be clobbered, and if so, asks whether this is OK */
{
  struct stat sz;
  bool retcode = FALSE;

  if (stat(filename,&sz)) {
    /* file doesn't exist, so it's ok to clobber it.  ;)  */
    return TRUE;
  }

  switch (st_priv.clobber_action) {
    case CLOBBER_ACTION_ASK:
      retcode = clobber_ask(filename);
      break;
    case CLOBBER_ACTION_ALWAYS:
      st_debug1("clobbering file: [%s]",filename);
      retcode = TRUE;
      break;
    case CLOBBER_ACTION_NEVER:
      st_debug1("refusing to clobber file: [%s]",filename);
      retcode = FALSE;
      break;
  }

  /* remove file if it exists, in case encoders check for its existence */
  if (retcode)
    remove_file(filename);

  return retcode;
}
