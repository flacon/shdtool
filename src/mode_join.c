/*  mode_join.c - join mode module
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
#include "mode.h"

CVSID("$Id: mode_join.c,v 1.110 2009/03/16 04:46:03 jason Exp $")

static bool join_main(int,char **);
static void join_help(void);

mode_module mode_join = {
  "join",
  "shnjoin",
  "Joins PCM WAVE data from multiple files into one",
  CVSIDSTR,
  TRUE,
  join_main,
  join_help
};

#define JOIN_PREFIX "joined"

enum {
  JOIN_UNKNOWN,
  JOIN_NOPAD,
  JOIN_PREPAD,
  JOIN_POSTPAD
};

static bool all_files_cd_quality = TRUE;
static int pad_bytes = 0;
static int numfiles;
static int pad_type = JOIN_UNKNOWN;

static wave_info **files;

static void join_help()
{
  st_info("Usage: %s [OPTIONS] [file1 file2 ...]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -b      pad the beginning of the joined file with silence\n");
  st_info("  -e      pad the end of the joined file with silence (default)\n");
  st_info("  -h      show this help screen\n");
  st_info("  -n      don't pad the joined file with silence\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = CURRENT_DIR;
  st_ops.output_prefix = JOIN_PREFIX;
  pad_type = JOIN_POSTPAD;

  while ((c = st_getopt(argc,argv,"ben")) != -1) {
    switch (c) {
      case 'b':
        pad_type = JOIN_PREPAD;
        break;
      case 'e':
        pad_type = JOIN_POSTPAD;
        break;
      case 'n':
        pad_type = JOIN_NOPAD;
        break;
    }
  }

  *first_arg = optind;
}

static bool do_join()
{
  int i,bytes_to_skip,bytes_to_xfer;
  proc_info output_proc;
  char outfilename[FILENAME_SIZE];
  wlong total=0;
  unsigned char header[CANONICAL_HEADER_SIZE];
  FILE *output;
  wave_info *joined_info;
  bool success;
  progress_info proginfo;

  success = FALSE;

  create_output_filename("","",outfilename);

  for (i=0;i<numfiles;i++)
    total += files[i]->data_size;

  if (all_files_cd_quality && (total % CD_BLOCK_SIZE) != 0) {
    pad_bytes = CD_BLOCK_SIZE - (total % CD_BLOCK_SIZE);
    if (JOIN_NOPAD != pad_type)
      total += pad_bytes;
  }

  if (NULL == (joined_info = new_wave_info(NULL))) {
    st_error("could not allocate memory for joined file information");
  }

  joined_info->chunk_size = total + CANONICAL_HEADER_SIZE - 8;
  joined_info->channels = files[0]->channels;
  joined_info->samples_per_sec = files[0]->samples_per_sec;
  joined_info->avg_bytes_per_sec = files[0]->avg_bytes_per_sec;
  joined_info->rate = files[0]->rate;
  joined_info->block_align = files[0]->block_align;
  joined_info->bits_per_sample = files[0]->bits_per_sample;
  joined_info->data_size = total;
  joined_info->wave_format = files[0]->wave_format;
  joined_info->problems = (files[0]->problems & PROBLEM_NOT_CD_QUALITY);

  if (PROB_ODD_SIZED_DATA(joined_info))
    joined_info->chunk_size++;

  joined_info->total_size = joined_info->chunk_size + 8;
  joined_info->length = joined_info->data_size / joined_info->rate;
  joined_info->exact_length = (double)joined_info->data_size / (double)joined_info->rate;

  length_to_str(joined_info);

  proginfo.initialized = FALSE;
  proginfo.prefix = "Joining";
  proginfo.clause = "-->";
  proginfo.filename1 = files[0]->filename;
  proginfo.filedesc1 = files[0]->m_ss;
  proginfo.filename2 = outfilename;
  proginfo.filedesc2 = joined_info->m_ss;
  proginfo.bytes_total = files[0]->total_size;

  prog_update(&proginfo);

  if (NULL == (output = open_output_stream(outfilename,&output_proc))) {
    st_error("could not open output file");
  }

  make_canonical_header(header,joined_info);

  if (write_n_bytes(output,header,CANONICAL_HEADER_SIZE,&proginfo) != CANONICAL_HEADER_SIZE) {
    prog_error(&proginfo);
    st_warning("error while writing %d-byte WAVE header",CANONICAL_HEADER_SIZE);
    goto cleanup;
  }

  if (all_files_cd_quality && (JOIN_PREPAD == pad_type) && pad_bytes) {
    if (pad_bytes != write_padding(output,pad_bytes,&proginfo)) {
      prog_error(&proginfo);
      st_warning("error while pre-padding with %d zero-bytes",pad_bytes);
      goto cleanup;
    }
  }

  for (i=0;i<numfiles;i++) {
    proginfo.bytes_total = files[i]->total_size;
    proginfo.filename1 = files[i]->filename;
    proginfo.filedesc1 = files[i]->m_ss;
    prog_update(&proginfo);

    if (!open_input_stream(files[i])) {
      prog_error(&proginfo);
      st_warning("could not reopen input file");
      goto cleanup;
    }

    bytes_to_skip = files[i]->header_size;

    while (bytes_to_skip > 0) {
      bytes_to_xfer = min(bytes_to_skip,CANONICAL_HEADER_SIZE);
      if (read_n_bytes(files[i]->input,header,bytes_to_xfer,NULL) != bytes_to_xfer) {
        prog_error(&proginfo);
        st_warning("error while reading %d bytes of data",bytes_to_xfer);
        goto cleanup;
      }
      bytes_to_skip -= bytes_to_xfer;
    }

    if (transfer_n_bytes(files[i]->input,output,files[i]->data_size,&proginfo) != files[i]->data_size) {
      prog_error(&proginfo);
      st_warning("error while transferring %lu bytes of data",files[i]->data_size);
      goto cleanup;
    }

    prog_success(&proginfo);

    close_input_stream(files[i]);
  }

  if (all_files_cd_quality && JOIN_POSTPAD == pad_type && pad_bytes) {
    if (pad_bytes != write_padding(output,pad_bytes,NULL)) {
      prog_error(&proginfo);
      st_warning("error while post-padding with %d zero-bytes",pad_bytes);
      goto cleanup;
    }
  }

  if ((JOIN_NOPAD == pad_type) && PROB_ODD_SIZED_DATA(joined_info) && (1 != write_padding(output,1,NULL))) {
    prog_error(&proginfo);
    st_warning("error while NULL-padding odd-sized data chunk");
    goto cleanup;
  }

  if (all_files_cd_quality) {
    if (JOIN_NOPAD != pad_type) {
      if (pad_bytes)
        st_info("%s-padded output file with %d zero-bytes.\n",((JOIN_PREPAD == pad_type)?"Pre":"Post"),pad_bytes);
      else
        st_info("No padding needed.\n");
    }
    else {
      st_info("Output file was not padded, ");
      if (pad_bytes)
        st_info("though it needs %d bytes of padding.\n",pad_bytes);
      else
        st_info("nor was it needed.\n");
    }
  }

  success = TRUE;

cleanup:
  if ((CLOSE_CHILD_ERROR_OUTPUT == close_output(output,output_proc)) || !success) {
    success = FALSE;
    remove_file(outfilename);
    st_error("failed to join files");
  }

  return success;
}

static void check_headers()
{
  int i;
  bool ba_warned = 0;

  for (i=0;i<numfiles;i++) {
    if (PROB_NOT_CD(files[i]))
      all_files_cd_quality = FALSE;

    if (PROB_HDR_INCONSISTENT(files[i]))
      st_error("file has an inconsistent header: [%s]",files[i]->filename);

    if (PROB_TRUNCATED(files[i]))
      st_error("file seems to be truncated: [%s]",files[i]->filename);
  }

  for (i=1;i<numfiles;i++) {
    if (files[i]->wave_format != files[0]->wave_format)
      st_error("WAVE format differs among these files");

    if (files[i]->channels != files[0]->channels)
      st_error("number of channels differs among these files");

    if (files[i]->samples_per_sec != files[0]->samples_per_sec)
      st_error("samples per second differs among these files");

    if (files[i]->avg_bytes_per_sec != files[0]->avg_bytes_per_sec)
      st_error("average bytes per second differs among these files");

    if (files[i]->bits_per_sample != files[0]->bits_per_sample)
      st_error("bits per sample differs among these files");

    if (files[i]->block_align != files[0]->block_align) {
      if (!ba_warned) {
        st_warning("block align differs among these files");
        ba_warned = TRUE;
      }
    }
  }
}

static bool process(int argc,char **argv,int start)
{
  int i;
  bool success;
  char *filename;

  input_init(start,argc,argv);
  input_read_all_files();
  numfiles = input_get_file_count();

  if (numfiles < 2)
    st_help("need two or more files to process");

  if (NULL == (files = malloc((numfiles + 1) * sizeof(wave_info *))))
    st_error("could not allocate memory for file info array");

  for (i=0;i<numfiles;i++) {
    filename = input_get_filename();
    if (NULL == (files[i] = new_wave_info(filename))) {
      st_error("could not open file: [%s]",filename);
    }
  }

  files[numfiles] = NULL;

  check_headers();

  reorder_files(files,numfiles);

  success = do_join();

  for (i=0;i<numfiles;i++)
    st_free(files[i]);

  st_free(files);

  return success;
}

static bool join_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
