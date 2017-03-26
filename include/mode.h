/*  mode.h - mode module definitions
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
 * $Id: mode.h,v 1.39 2009/03/30 06:44:42 jason Exp $
 */

#ifndef __MODE_H__
#define __MODE_H__

#include <stdlib.h>
#include <unistd.h>
#include "format-types.h"
#include "mode-types.h"
#include "module.h"
#include "binary.h"

/* default output directories */
#define CURRENT_DIR    "."
#define INPUT_FILE_DIR ""

/* various buffer sizes */
#define BUF_SIZE  2048
#define XFER_SIZE 262144

/* error codes */
#define ST_EXIT_SUCCESS 0
#define ST_EXIT_ERROR   1
#define ST_EXIT_QUIT    255

/* global mode-accessible options */
extern global_opts st_ops;

/* function to open an input stream and skip past the ID3v2 tag, if one exists */
bool open_input_stream(wave_info *);

/* function to handle command-line option parsing with global (i.e. non-mode-specific) options */
int st_getopt(int,char **,char *);

/* global usage screen */
void st_global_usage();

/* returns program name ("prog mode" or "alias") */
char *st_progname(void);

/* function to convert a file's length to m:ss or m:ss.ff format */
void length_to_str(wave_info *);

/* close a file descriptor, and wait for a child process if necessary */
int close_and_wait(FILE *,proc_info *,int,format_module *);
#define close_input(a,b)       close_and_wait(a,&b,CHILD_INPUT,NULL)
#define close_output(a,b)      close_and_wait(a,&b,CHILD_OUTPUT,NULL)
#define close_input_stream(a)  close_and_wait(a->input,&a->input_proc,CHILD_INPUT,a->input_format)
#define close_output_stream(a) close_and_wait(a->output,&a->output_proc,CHILD_OUTPUT,NULL)

/* function to discard the WAVE header, leaving the file pointer at the beginning of the audio data */
void discard_header(wave_info *);

/* wrapper function to name output filename based on input filename and extension */
void create_output_filename(char *,char *,char *);

/* wrapper function to open an output stream */
FILE *open_output_stream(char *,proc_info *);

/* function to determine if two filenames point to the same file */
int files_are_identical(char *,char *);

/* function to remove a file if it exists */
void remove_file(char *);

/* a simple menu system to alter the order in which given input files will be processed */
void alter_file_order(wave_info **,int);

/* function to reorder files based on user preference */
void reorder_files(wave_info **,int);

/* functions to aid in parsing input length formats */
wlong smrt_parse(unsigned char *,wave_info *);

/* function to determine whether odd-sized data chunks are NULL-padded to an even length */
bool odd_sized_data_chunk_is_null_padded(wave_info *);

/* functions for building argument lists in format modules */
void arg_reset(child_args *);
void arg_add(child_args *,char *);
void arg_replace(child_args *,int,char *);

/* functions for progress output */
void prog_update(progress_info *);
void prog_success(progress_info *);
void prog_error(progress_info *);

/* functions for managing the input file source */
void input_init(int,int,char **);
char *input_get_filename();
void input_read_all_files();
int input_get_file_count();

#endif
