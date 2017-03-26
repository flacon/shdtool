/*  core_output.c - generic output functions
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
#include <stdarg.h>
#include "shntool.h"

CVSID("$Id: core_output.c,v 1.36 2009/03/11 17:18:01 jason Exp $")

static char msgbuf[BUF_SIZE];
static char errbuf[BUF_SIZE];

void st_output(char *msg, ...)
{
  va_list args;

  va_start(args,msg);

  st_vsnprintf(msgbuf,BUF_SIZE,msg,args);

  fprintf(stdout,"%s",msgbuf);

  va_end(args);
}

void st_info(char *msg, ...)
{
  va_list args;

  if (st_priv.suppress_stderr)
    return;

  va_start(args,msg);

  st_vsnprintf(errbuf,BUF_SIZE,msg,args);

  fprintf(stderr,"%s",errbuf);

  va_end(args);
}

static void print_prefix()
{
  if (st_priv.is_aliased || NULL == st_priv.progmode)
    fprintf(stderr,"%s: ",st_priv.progname);
  else
    fprintf(stderr,"%s [%s]: ",st_priv.progname,st_priv.progmode);
}

static void print_msgtype(char *msgtype,int line)
{
  int i;

  if (0 == line) {
    fprintf(stderr,"%s",msgtype);
  }
  else {
    for (i=0;i<strlen(msgtype);i++)
      fprintf(stderr," ");
  }
}

static void print_lines(char *msgtype,char *msg)
{
  int line = 0;
  char *head, *tail;

  head = tail = msgbuf;
  while (*head) {
    if ('\n' == *head) {
      *head = 0;

      print_prefix();
      print_msgtype(msgtype,line);
      fprintf(stderr,"%s\n",tail);

      tail = head + 1;
      line++;
    }
    head++;
  }

  print_prefix();
  print_msgtype(msgtype,line);
  fprintf(stderr,"%s\n",tail);
}

void st_error(char *msg, ...)
{
  va_list args;

  va_start(args,msg);

  st_vsnprintf(msgbuf,BUF_SIZE,msg,args);

  print_lines("error: ",msgbuf);

  va_end(args);

  exit(ST_EXIT_ERROR);
}

void st_help(char *msg, ...)
{
  va_list args;

  va_start(args,msg);

  st_vsnprintf(msgbuf,BUF_SIZE,msg,args);

  print_lines("error: ",msgbuf);

  va_end(args);

  print_prefix();
  fprintf(stderr,"\n");
  print_prefix();
  fprintf(stderr,"type '%s -h' for help\n",st_priv.fullprogname);

  exit(ST_EXIT_ERROR);
}

void st_warning(char *msg, ...)
{
  va_list args;

  if (st_priv.suppress_warnings || st_priv.suppress_stderr)
    return;

  va_start(args,msg);

  st_vsnprintf(msgbuf,BUF_SIZE,msg,args);

  print_lines("warning: ",msgbuf);

  va_end(args);
}

void st_debug_internal(int level,char *msg,va_list args)
{
  char debugprefix[16];

  if (level > st_priv.debug_level)
    return;

  st_vsnprintf(msgbuf,BUF_SIZE,msg,args);

  st_snprintf(debugprefix,16,"debug%d: ",level);

  print_lines(debugprefix,msgbuf);
}

void st_debug1(char *msg, ...)
{
  va_list args;

  va_start(args,msg);
  st_debug_internal(1,msg,args);
  va_end(args);
}

void st_debug2(char *msg, ...)
{
  va_list args;

  va_start(args,msg);
  st_debug_internal(2,msg,args);
  va_end(args);
}

void st_debug3(char *msg, ...)
{
  va_list args;

  va_start(args,msg);
  st_debug_internal(3,msg,args);
  va_end(args);
}
