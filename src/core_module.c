/*  core_module.c - public functions for both mode and format modules
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
#include "shntool.h"

CVSID("$Id: core_module.c,v 1.148 2009/03/11 17:18:01 jason Exp $")

/* public functions */

unsigned long check_for_id3v2_tag(FILE *f)
{
  id3v2_header id3v2hdr;
  unsigned long tag_size;

  /* read an ID3v2 header's size worth of data */
  if (sizeof(id3v2_header) != fread(&id3v2hdr,1,sizeof(id3v2_header),f)) {
    return 0;
  }

  /* verify this is an ID3v2 header */
  if (tagcmp((unsigned char *)id3v2hdr.magic,(unsigned char *)ID3V2_MAGIC) ||
      0xff == id3v2hdr.version[0] || 0xff == id3v2hdr.version[1] ||
      0x80 <= id3v2hdr.size[0] || 0x80 <= id3v2hdr.size[1] ||
      0x80 <= id3v2hdr.size[2] || 0x80 <= id3v2hdr.size[3])
  {
    return 0;
  }

  /* calculate and return ID3v2 tag size */
  tag_size = synchsafe_int_to_ulong(id3v2hdr.size);

  return tag_size;
}

FILE *open_input_internal(char *filename,bool *file_has_id3v2_tag,wlong *id3v2_tag_size)
/* opens a file, and if it contains an ID3v2 tag, skips past it */
{
  FILE *f;
  unsigned long tag_size;

  if (NULL == (f = fopen(filename,"rb"))) {
    return NULL;
  }

  if (file_has_id3v2_tag)
    *file_has_id3v2_tag = FALSE;

  if (id3v2_tag_size)
    *id3v2_tag_size = 0;

  /* check for ID3v2 tag on input */
  if (0 == (tag_size = check_for_id3v2_tag(f))) {
    fclose(f);
    return fopen(filename,"rb");
  }

  if (file_has_id3v2_tag)
    *file_has_id3v2_tag = TRUE;

  if (id3v2_tag_size)
    *id3v2_tag_size = (wlong)(tag_size + sizeof(id3v2_header));

  st_debug1("discarding %lu-byte ID3v2 tag at beginning of file: [%s]",tag_size+sizeof(id3v2_header),filename);

  if (fseek(f,(long)tag_size,SEEK_CUR)) {
    st_warning("error while discarding ID3v2 tag in file: [%s]",filename);
    fclose(f);
    return fopen(filename,"rb");
  }

  return f;
}

FILE *open_output(char *filename)
{
  return fopen(filename,"wb");
}

char *scan_env(char *envvar)
/* function to scan environment and return a pointer to the variable if it exists and is nonempty, otherwise returns NULL */
{
  char *envp;

  envp = getenv(envvar);

  if ((NULL == envp) || (!strcmp(envp,"")))
    return NULL;

  return envp;
}

void trim(char *str)
/* function to trim carriage returns and newlines from the end of strings */
{
  int i;

  i = strlen(str) - 1;

  while ((i >= 0) && (('\n' == str[i]) || ('\r' == str[i]))) {
    str[i] = 0;
    i--;
  }
}

char *basename(char *filename)
/* function to return the basename of a file (filename minus any directory names) */
{
  char *base;

  if ((base = strrchr(filename,PATHSEPCHAR)))
    base++;
  else
    base = filename;

  return base;
}

char *extname(char *filename)
/* function to return the extension of a file, if any */
{
  char *base,*ext;

  base = basename(filename);

  if ((ext = strrchr(base,'.')))
    ext++;
  else
    ext = NULL;

  return ext;
}
