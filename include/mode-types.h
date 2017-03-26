/*  mode-types.h - mode module type definitions
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
 * $Id: mode-types.h,v 1.19 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __MODE_TYPES_H__
#define __MODE_TYPES_H__

#include "module-types.h"

typedef struct _mode_module {
  char  *const name;              /* mode name, specified on command line */
  char  *const alias;             /* alternate name that invokes this mode */
  char  *const description;       /* one line description of this mode */
  char  *const cvsid;             /* CVS revision (used to prevent it from being stripped) */
  bool  const creates_files;      /* does this mode create output files? */
  bool (*run_main)(int,char **);  /* main() function for this mode */
  void (*run_help)(void);         /* help() function for this mode */
} mode_module;

/* mode-accessible global options */
typedef struct _global_opts {
  char *output_directory;
  char *output_prefix;
  char *output_postfix;
  format_module *output_format;
} global_opts;

/* mode progress status output */
typedef struct _progress_info {
  char *prefix;
  char *clause;
  char *filename1;
  char *filedesc1;
  char *filename2;
  char *filedesc2;
  wlong bytes_written;
  wlong bytes_total;
  int dot_step;
  int percent;
  int last_percent;
  bool initialized;
  bool progress_shown;
} progress_info;

/* filename ordering options */
typedef enum {
  ORDER_AS_IS,
  ORDER_ASK,
  ORDER_ASCII,
  ORDER_NATURAL
} ordering_types;

#endif
