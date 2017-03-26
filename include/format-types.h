/*  format-types.h - format module type definitions
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
 * $Id: format-types.h,v 1.17 2009/03/11 17:18:01 jason Exp $
 */

#ifndef __FORMAT_TYPES_H__
#define __FORMAT_TYPES_H__

#include <stdio.h>
#include "module-types.h"
#include "wave.h"

#define MAX_CHILD_ARGS 256

/* child argument lists */
typedef struct _child_args {
  int num_args;
  char *args[MAX_CHILD_ARGS];
} child_args;

typedef struct _format_module {
  /* set at compile time */
  char   *const name;                        /* format name, specified on command line in certain modes */
  char   *const description;                 /* one line description of this format */
  char   *const cvsid;                       /* CVS revision (used to prevent it from being stripped) */
  bool    const supports_input;              /* does this format support input? */
  bool    const supports_output;             /* does this format support output? */
  bool    const is_translated;               /* is this file format a simple translation (e.g. byte-swap) of WAV? */
  bool    const is_compressed;               /* is this file format a compression format? */
  bool    const remove_output_file;          /* remove output file before opening it for output?  (prevents some encoders from complaining that the file already exists) */
  bool    const kill_when_input_done;        /* kill() decoder when finished reading input?  (prevents delays introduced by decoders that don't exit when we close stdout) */
  char   *const stdin_for_id3v2_kluge;       /* if not NULL, try to skip id3v2 tags on input files by passing data on decoder's stdin using this as the stdin argument */
  char   *const magic;                       /* if not NULL, shntool will check for this string at position magic_offset in the input file */
  int     const magic_offset;                /* offset within the file at which the magic string is expected to occur */

  /* can be changed at run time */
  char   *extension;                         /* extension to append to files created by this module */
  char   *decoder;                           /* name of external program to handle decoding of this format - set to NULL if not applicable */
  char   *decoder_args;                      /* decoder arguments */
  char   *encoder;                           /* name of external program to handle encoding of this format - set to NULL if not applicable */
  char   *encoder_args;                      /* encoder arguments */

  /* optional functions - set to NULL if not applicable */
  bool  (*is_our_file)(char *);              /* routine to determine whether the given file belongs to this format plugin */
  FILE *(*input_func)(char *,proc_info *);   /* routine to open a file of this format for input - if NULL, shntool will launch the decoder */
  FILE *(*output_func)(char *,proc_info *);  /* routine to open a file of this format for output - if NULL, shntool will launch the encoder */
  void  (*extra_info)(char *);               /* routine to display extra information in info mode */
  void  (*create_output_filename)(char *);   /* routine to create a custom output filename */
  bool  (*input_header_kluge)(unsigned char *,struct _wave_info *);  /* routine to determine correct header info for when decoders are unable to do so themselves */

  /* internal argument lists (do not assign these in format modules) */
  child_args input_args_template;           /* input argument template (filled out by shntool, based on default_decoder_args) */
  child_args input_args;                    /* input arguments, filled out (used by shntool when launching decoder) */
  child_args output_args_template;          /* output argument template (filled out by shntool, based on default_encoder_args) */
  child_args output_args;                   /* output arguments, filled out (used by shntool when launching encoder) */
} format_module;

/* child process types */
typedef enum {
  CHILD_INPUT,
  CHILD_OUTPUT
} child_types;

/* closed child process return codes */
typedef enum {
  CLOSE_SUCCESS,
  CLOSE_CHILD_ERROR_INPUT,
  CLOSE_CHILD_ERROR_OUTPUT
} close_retcodes;

#endif
