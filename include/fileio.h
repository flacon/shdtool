/*  fileio.h - file I/O function definitions
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
 * $Id: fileio.h,v 1.29 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __FILEIO_H__
#define __FILEIO_H__

#include <stdio.h>
#include "mode-types.h"

/* writes the specified number of zero bytes to the file descriptor given */
int write_padding(FILE *,int,progress_info *);

/* reads n bytes from a file into a buffer */
int read_n_bytes(FILE *,unsigned char *,int,progress_info *);

/* writes n bytes from a buffer into a file */
int write_n_bytes(FILE *,unsigned char *,int,progress_info *);

/* transfers n bytes from a file into another file */
unsigned long transfer_n_bytes_internal(FILE *,FILE *,FILE *,unsigned long,progress_info *);
#define transfer_n_bytes(a,b,c,d)       transfer_n_bytes_internal(a,b,NULL,c,d)
#define transfer_n_bytes2(a,b,c,d,e)    transfer_n_bytes_internal(a,b,c,d,e)

/* reads an unsigned long in big- and/or little-endian format from a file descriptor */
bool read_value_long(FILE * file,unsigned long *,unsigned long *,unsigned char *);
#define read_tag(f,t)     read_value_long(f,NULL,NULL,t)
#define read_be_long(f,b) read_value_long(f,b,NULL,NULL)
#define read_le_long(f,l) read_value_long(f,NULL,l,NULL)

/* reads an unsigned short in big- and/or little-endian format from a file descriptor */
bool read_value_short(FILE * file,unsigned short *,unsigned short *);
#define read_be_short(f,b) read_value_short(f,b,NULL)
#define read_le_short(f,l) read_value_short(f,NULL,l)

#endif
