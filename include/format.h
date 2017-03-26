/*  format.h - format module definitions
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
 * $Id: format.h,v 1.35 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __FORMAT_H__
#define __FORMAT_H__

#include "format-types.h"
#include "module.h"

#define FILENAME_PLACEHOLDER "%f"

/* copies an arbitrary-length tag, without the NULL byte */
void tagcpy(unsigned char *,unsigned char *);

/* compares what was received to the expected tag */
int tagcmp(unsigned char *,unsigned char *);

/* function to check if a file name is about to be clobbered, and if so, asks whether this is OK */
int clobber_check(char *);

/* find an output format module with the given name */
format_module *find_format(char *);

/* launch encoders/decoders */
FILE *launch_input(format_module *,char *,proc_info *);
FILE *launch_output(format_module *,char *,proc_info *);

/* generic check for "magic" strings at known offsets */
bool check_for_magic(char *,char *,int);

#endif
