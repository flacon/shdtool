/*  core_convert.c - little-endian conversion functions
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

#include "shntool.h"

CVSID("$Id: core_convert.c,v 1.21 2009/03/11 17:18:01 jason Exp $")

unsigned long uchar_to_ulong_le(unsigned char * buf)
/* converts 4 bytes stored in little-endian format to an unsigned long */
{
  return (unsigned long)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

unsigned short uchar_to_ushort_le(unsigned char * buf)
/* converts 2 bytes stored in little-endian format to an unsigned short */
{
  return (unsigned short)(buf[0] | (buf[1] << 8));
}

void ulong_to_uchar_le(unsigned char * buf,unsigned long num)
/* converts an unsigned long to 4 bytes stored in little-endian format */
{
  buf[0] = (unsigned char)(num);
  buf[1] = (unsigned char)(num >> 8);
  buf[2] = (unsigned char)(num >> 16);
  buf[3] = (unsigned char)(num >> 24);
}

void ushort_to_uchar_le(unsigned char * buf,unsigned short num)
/* converts an unsigned short to 2 bytes stored in little-endian format */
{
  buf[0] = (unsigned char)(num);
  buf[1] = (unsigned char)(num >> 8);
}

unsigned long uchar_to_ulong_be(unsigned char * buf)
/* converts 4 bytes stored in big-endian format to an unsigned long */
{
  return (unsigned long)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

unsigned short uchar_to_ushort_be(unsigned char * buf)
/* converts 2 bytes stored in big-endian format to an unsigned short */
{
  return (unsigned short)((buf[0] << 8) | buf[1]);
}

void ulong_to_uchar_be(unsigned char * buf,unsigned long num)
/* converts an unsigned long to 4 bytes stored in big-endian format */
{
  buf[0] = (unsigned char)(num >> 24);
  buf[1] = (unsigned char)(num >> 16);
  buf[2] = (unsigned char)(num >> 8);
  buf[3] = (unsigned char)(num);
}

void ushort_to_uchar_be(unsigned char * buf,unsigned short num)
/* converts an unsigned short to 2 bytes stored in big-endian format */
{
  buf[0] = (unsigned char)(num >> 8);
  buf[1] = (unsigned char)(num);
}

unsigned long synchsafe_int_to_ulong(unsigned char *buf)
/* converts 4 bytes stored in synchsafe integer format to an unsigned long */
{
  return (unsigned long)(((buf[0] & 0x7f) << 21) | ((buf[1] & 0x7f) << 14) | ((buf[2] & 0x7f) << 7) | (buf[3] & 0x7f));
}
