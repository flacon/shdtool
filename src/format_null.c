/*  format_null.c - /dev/null format module
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
#include "format.h"

CVSID("$Id: format_null.c,v 1.44 2009/03/11 17:18:01 jason Exp $")

static FILE *open_for_output(char *,proc_info *);
static void create_output_filename(char *);

format_module format_null = {
  "null",
  "Sends output to " NULLDEVICE,
  CVSIDSTR,
  FALSE,
  TRUE,
  FALSE,
  FALSE,
  FALSE,
  FALSE,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  open_for_output,
  NULL,
  create_output_filename,
  NULL
};

static FILE *open_for_output(char *filename,proc_info *pinfo)
{
  pinfo->pid = NO_CHILD_PID;
  return open_output(NULLDEVICE);
}

static void create_output_filename(char *outfilename)
{
  strcpy(outfilename,NULLDEVICE);
}
