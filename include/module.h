/*  module.h - definitions common to mode and format modules
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
 * $Id: module.h,v 1.15 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __MODULE_H__
#define __MODULE_H__

#include <stdio.h>
#include "fileio.h"
#include "output.h"
#include "wave.h"
#include "module-types.h"

/* macro to set cvsid string */
#define CVSID(x) static const char cvsid[] = x;
#define CVSIDSTR ((char *const)&cvsid)

/* misc definitions */
#define FILENAME_SIZE 2048
#define NO_CHILD_PID -740

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define st_free(x) if (x) { free(x); x = NULL; }

/* boolean definitions */
#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define TRUE  (1==1)
#define FALSE (0==1)

/* function to open an input file for reading and skip past the ID3v2 tag, if one exists */
FILE *open_input_internal(char *,bool *,wlong *);
#define open_input(a) open_input_internal(a,NULL,NULL)

/* function to open an output file for writing */
FILE *open_output(char *);

/* function to scan environment and return a pointer to the variable if it exists and is nonempty, otherwise returns NULL */
char *scan_env(char *);

/* function to determine whether the data on the given file pointer contains an ID3v2 tag */
unsigned long check_for_id3v2_tag(FILE *);

/* function to trim carriage returns and newlines from the end of strings */
void trim(char *);

/* function to return a pointer to the filename with all directory components removed */
char *basename(char *);

/* function to return a pointer to the extension of a filename, if one exists */
char *extname(char *);

/* replacement snprintf */
void st_snprintf(char *,int,char *,...);

#endif
