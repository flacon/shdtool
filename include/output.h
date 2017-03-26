/*  output.h - output functions
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
 * $Id: output.h,v 1.11 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __OUTPUT_H__
#define __OUTPUT_H__

void st_output(char *, ...);
void st_info(char *, ...);
void st_warning(char *, ...);
void st_error(char *, ...);
void st_help(char *, ...);
void st_debug1(char *, ...);
void st_debug2(char *, ...);
void st_debug3(char *, ...);

#endif
