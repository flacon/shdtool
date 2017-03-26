/*  module-types.h - type definitions common to both modes and formats
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
 * $Id: module-types.h,v 1.8 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __MODULE_TYPES_H__
#define __MODULE_TYPES_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef WIN32
#define NULLDEVICE  "NUL"
#define TERMIDEVICE "CON"
#define TERMODEVICE "CON"
#define PATHSEPCHAR '\\'
#else
#define NULLDEVICE  "/dev/null"
#define TERMIDEVICE "/dev/stdin"
#define TERMODEVICE "/dev/stdout"
#define PATHSEPCHAR '/'
#endif

/* boolean type */
typedef int bool;

/* wtypes */
typedef unsigned long wlong;
typedef unsigned short wshort;
typedef unsigned int wint;

/* process information */
typedef struct _proc_info {
  int pid;
#ifdef WIN32
  HANDLE hProcess;
#endif
} proc_info;

#endif
