/*  mode_fix.c - fix mode module
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

#include "mode.h"

CVSID("$Id: mode_fix.c,v 1.115 2009/03/16 04:46:03 jason Exp $")

static bool fix_main(int,char **);
static void fix_help(void);

mode_module mode_fix = {
  "fix",
  "shnfix",
  "Fixes sector-boundary problems with CD-quality PCM WAVE data",
  CVSIDSTR,
  TRUE,
  fix_main,
  fix_help
};

#define FIX_POSTFIX "-fixed"

typedef enum {
  SHIFT_UNKNOWN,
  SHIFT_BACKWARD,
  SHIFT_FORWARD,
  SHIFT_ROUND
} fix_shifts;

static bool pad = TRUE;
static bool skip = TRUE;
static bool check_only = FALSE;
static int pad_bytes = 0;
static int desired_shift = SHIFT_UNKNOWN;
static int numfiles;

static wave_info **files;

static void fix_help()
{
  st_info("Usage: %s [OPTIONS] [file1 ...]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -b      shift track breaks backward to previous sector boundary (default)\n");
  st_info("  -c      check whether fixing is needed\n");
  st_info("  -f      shift track breaks forward to next sector boundary\n");
  st_info("  -h      show this help screen\n");
  st_info("  -k      don't skip initial unchanged files\n");
  st_info("  -n      don't pad the last file with silence\n");
  st_info("  -u      round track breaks to the nearest sector boundary\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  st_ops.output_directory = CURRENT_DIR;
  st_ops.output_postfix = FIX_POSTFIX;
  desired_shift = SHIFT_BACKWARD;

  while ((c = st_getopt(argc,argv,"bcfknu")) != -1) {
    switch (c) {
      case 'b':
        desired_shift = SHIFT_BACKWARD;
        break;
      case 'c':
        check_only = TRUE;
        break;
      case 'f':
        desired_shift = SHIFT_FORWARD;
        break;
      case 'k':
        skip = FALSE;
        break;
      case 'n':
        pad = FALSE;
        break;
      case 'u':
        desired_shift = SHIFT_ROUND;
        break;
    }
  }

  *first_arg = optind;
}

static void calculate_breaks_backward()
{
  int i,remainder = 0;
  wlong tmp,begin_before = 0,begin_after = 0;

  for (i=0;i<numfiles-1;i++) {
    files[i]->beginning_byte = begin_before;
    files[i]->new_beginning_byte = begin_after;

    tmp = files[i]->data_size + remainder;
    remainder = tmp % CD_BLOCK_SIZE;
    files[i]->new_data_size = tmp - remainder;

    begin_before += files[i]->data_size;
    begin_after += files[i]->new_data_size;
  }
  files[numfiles-1]->new_data_size = files[numfiles-1]->data_size + remainder;
  files[numfiles-1]->beginning_byte = begin_before;
  files[numfiles-1]->new_beginning_byte = begin_after;
}

static void calculate_breaks_forward()
{
  int i,used = 0,remainder;
  wlong tmp,begin_before = 0,begin_after = 0;

  for (i=0;i<numfiles-1;i++) {
    files[i]->beginning_byte = begin_before;
    files[i]->new_beginning_byte = begin_after;

    tmp = files[i]->data_size - used;
    remainder = tmp % CD_BLOCK_SIZE;
    if (remainder) {
      used = CD_BLOCK_SIZE - remainder;
      files[i]->new_data_size = tmp + used;
    }
    else {
      used = 0;
      files[i]->new_data_size = tmp;
    }

    begin_before += files[i]->data_size;
    begin_after += files[i]->new_data_size;
  }
  files[numfiles-1]->new_data_size = files[numfiles-1]->data_size - used;
  files[numfiles-1]->beginning_byte = begin_before;
  files[numfiles-1]->new_beginning_byte = begin_after;
}

static void calculate_breaks_round()
{
  int i,give_or_take = 0;
  wlong tmp,how_much,begin_before = 0,begin_after = 0;

  for (i=0;i<numfiles-1;i++) {
    files[i]->beginning_byte = begin_before;
    files[i]->new_beginning_byte = begin_after;

    tmp = files[i]->data_size + give_or_take;
    how_much = (((tmp + CD_BLOCK_SIZE/2) / CD_BLOCK_SIZE) * CD_BLOCK_SIZE);
    give_or_take = tmp - how_much;
    files[i]->new_data_size = how_much;

    begin_before += files[i]->data_size;
    begin_after += files[i]->new_data_size;
  }
  files[numfiles-1]->new_data_size = files[numfiles-1]->data_size + give_or_take;
  files[numfiles-1]->beginning_byte = begin_before;
  files[numfiles-1]->new_beginning_byte = begin_after;
}

static void calculation_sanity_check()
{
  int i;
  wlong old_total = 0,new_total = 0;

  for (i=0;i<numfiles;i++) {
    old_total += files[i]->data_size;
    new_total += files[i]->new_data_size;
  }

  if (old_total != new_total) {
    st_warning("total WAVE data size differs from newly calculated total --\n"
               "please file a bug report with the following data:");

    st_info("\n");
    st_info("Shift type: %s\n",(SHIFT_BACKWARD == desired_shift)?"backward":
                                      ((SHIFT_FORWARD == desired_shift)?"forward":
                                      ((SHIFT_ROUND == desired_shift)?"round":"unknown")));

    st_info("\n");
    for (i=0;i<numfiles;i++)
      st_info("file %2d:  data size = %10lu, new data size = %10lu\n",i+1,files[i]->data_size,files[i]->new_data_size);

    st_info("\n");
    st_info("totals :  data size = %10lu, new data size = %10lu\n",old_total,new_total);

    exit(ST_EXIT_ERROR);
  }
}

static bool reopen_input_file(int i,progress_info *proginfo)
{
  unsigned char *header;

  if (!open_input_stream(files[i])) {
    prog_error(proginfo);
    st_warning("could not reopen input file: [%s]",files[i]->filename);
    return FALSE;
  }

  if (NULL == (header = malloc(files[i]->header_size * sizeof(unsigned char)))) {
    prog_error(proginfo);
    st_warning("could not allocate %d-byte WAVE header",files[i]->header_size);
    return FALSE;
  }

  if (read_n_bytes(files[i]->input,header,files[i]->header_size,NULL) != files[i]->header_size) {
    prog_error(proginfo);
    st_warning("error while reading %d-byte WAVE header",files[i]->header_size);
    st_free(header);
    return FALSE;
  }

  st_free(header);

  return TRUE;
}

static bool open_this_file(int i,char *outfilename,progress_info *proginfo)
{
  unsigned char header[CANONICAL_HEADER_SIZE];

  create_output_filename(files[i]->filename,files[i]->input_format->extension,outfilename);

  proginfo->filename1 = files[i]->filename;
  proginfo->filedesc1 = files[i]->m_ss;
  proginfo->filename2 = outfilename;

  if (NULL == (files[i]->output = open_output_stream(outfilename,&files[i]->output_proc))) {
    prog_error(proginfo);
    st_error("could not open output file: [%s]",outfilename);
  }

  make_canonical_header(header,files[i]);

  if ((numfiles - 1 == i) && pad)
    put_data_size(header,CANONICAL_HEADER_SIZE,files[i]->new_data_size+pad_bytes);
  else {
    put_data_size(header,CANONICAL_HEADER_SIZE,files[i]->new_data_size);
    if (files[i]->new_data_size & 1)
      put_chunk_size(header,(files[i]->new_data_size + 1) + CANONICAL_HEADER_SIZE - 8);
  }

  if (write_n_bytes(files[i]->output,header,CANONICAL_HEADER_SIZE,proginfo) != CANONICAL_HEADER_SIZE) {
    prog_error(proginfo);
    st_warning("error while writing %d-byte WAVE header",CANONICAL_HEADER_SIZE);
    return FALSE;
  }

  proginfo->filename2 = outfilename;
  proginfo->bytes_total = files[i]->new_data_size + CANONICAL_HEADER_SIZE;

  return TRUE;
}

static bool write_fixed_files()
{
  int cur_input,cur_output;
  unsigned long bytes_have,bytes_needed,bytes_to_xfer;
  char outfilename[FILENAME_SIZE];
  bool success;
  progress_info proginfo;

  success = FALSE;

  cur_input = cur_output = 0;
  bytes_have = (unsigned long)files[cur_input]->data_size;
  bytes_needed = (unsigned long)files[cur_output]->new_data_size;

  proginfo.initialized = FALSE;
  proginfo.prefix = "Fixing";
  proginfo.clause = "-->";
  proginfo.filename1 = files[0]->filename;
  proginfo.filedesc1 = files[0]->m_ss;
  proginfo.filename2 = NULL;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = 1;

  if (!open_this_file(cur_output,outfilename,&proginfo))
    goto cleanup;

  if (!reopen_input_file(cur_input,&proginfo))
    goto cleanup;

  while (cur_input < numfiles && cur_output < numfiles) {
    bytes_to_xfer = min(bytes_have,bytes_needed);

    if (transfer_n_bytes(files[cur_input]->input,files[cur_output]->output,bytes_to_xfer,&proginfo) != bytes_to_xfer) {
      prog_error(&proginfo);
      st_warning("error while transferring %lu bytes of data",bytes_to_xfer);
      goto cleanup;
    }

    bytes_have -= bytes_to_xfer;
    bytes_needed -= bytes_to_xfer;

    if (0 == bytes_have) {
      close_input_stream(files[cur_input]);
      files[cur_input]->input = NULL;
      cur_input++;
      if (cur_input < numfiles) {
        bytes_have = (unsigned long)files[cur_input]->data_size;

        if (!reopen_input_file(cur_input,&proginfo))
          goto cleanup;
      }
    }

    if (0 == bytes_needed) {
      prog_success(&proginfo);

      if (numfiles - 1 == cur_output) {
        if (pad) {
          if (pad_bytes) {
            if (pad_bytes != write_padding(files[cur_output]->output,pad_bytes,NULL)) {
              prog_error(&proginfo);
              st_warning("error while padding with %d zero-bytes",pad_bytes);
              goto cleanup;
            }
            st_info("Padded last file with %d zero-bytes.\n",pad_bytes);
          }
          else
            st_info("No padding needed.\n");
        }
        else {
          st_info("Last file was not padded, ");
          if (pad_bytes)
            st_info("though it needs %d bytes of padding.\n",pad_bytes);
          else
            st_info("nor was it needed.\n");

          if ((files[cur_output]->new_data_size & 1) && (1 != write_padding(files[cur_output]->output,1,NULL))) {
            prog_error(&proginfo);
            st_warning("error while NULL-padding odd-sized data chunk");
            goto cleanup;
          }
        }
      }

      close_output_stream(files[cur_output]);
      files[cur_output]->output = NULL;
      cur_output++;
      if (cur_output < numfiles) {
        bytes_needed = (unsigned long)files[cur_output]->new_data_size;

        if (!open_this_file(cur_output,outfilename,&proginfo))
          goto cleanup;
      }
    }
  }

  success = TRUE;

cleanup:
  if (!success) {
    close_output_stream(files[cur_output]);
    remove_file(outfilename);
    st_error("failed to fix files");
  }

  return success;
}

static bool process(int argc,char **argv,int start)
{
  int i,j,remainder;
  bool needs_fixing = FALSE,found_errors = FALSE,success;
  char *filename;

  input_init(start,argc,argv);
  input_read_all_files();
  numfiles = input_get_file_count();

  if (numfiles < 1)
    st_help("need one or more files to process");

  if (NULL == (files = malloc((numfiles + 1) * sizeof(wave_info *))))
    st_error("could not allocate memory for file info array");

  for (i=0;i<numfiles;i++) {
    filename = input_get_filename();
    if (NULL == (files[i] = new_wave_info(filename))) {
      st_error("could not open file: [%s]",filename);
    }
  }

  files[numfiles] = NULL;

  /* validate files */
  for (i=0;i<numfiles;i++) {
    if (PROB_NOT_CD(files[i])) {
      st_warning("file is not CD-quality: [%s]",files[i]->filename);
      found_errors = TRUE;
    }
    if (PROB_HDR_INCONSISTENT(files[i])) {
      st_warning("file has an inconsistent header: [%s]",files[i]->filename);
      found_errors = TRUE;
    }
    if (PROB_TRUNCATED(files[i])) {
      st_warning("file seems to be truncated: [%s]",files[i]->filename);
      found_errors = TRUE;
    }
    if (PROB_BAD_BOUND(files[i]))
      needs_fixing = TRUE;
  }

  if (found_errors)
    st_error("could not fix files due to errors, see above");

  if (check_only)
    exit(needs_fixing ? ST_EXIT_SUCCESS : ST_EXIT_ERROR);

  if (!needs_fixing)
    st_error("everything seems fine, no need for fixing");

  reorder_files(files,numfiles);

  i = 0;
  while (!PROB_BAD_BOUND(files[i]))
    i++;

  if (skip) {
    if (i != 0) {
      st_warning("skipping first %d file%s because %s would not be changed",i,(1==i)?"":"s",(1==i)?"it":"they");
      for (j=0;j<i;j++)
        st_free(files[j]);
      for (j=i;j<numfiles;j++) {
        files[j-i] = files[j];
        files[j] = NULL;
      }
      numfiles -= i;
    }
  }

  if (numfiles < 1)
    st_error("need one or more files to process");

  switch (desired_shift) {
    case SHIFT_BACKWARD:
      calculate_breaks_backward();
      break;
    case SHIFT_FORWARD:
      calculate_breaks_forward();
      break;
    case SHIFT_ROUND:
      calculate_breaks_round();
      break;
  }

  calculation_sanity_check();

  remainder = files[numfiles-1]->new_data_size % CD_BLOCK_SIZE;
  if (remainder)
    pad_bytes = CD_BLOCK_SIZE - remainder;

  success = write_fixed_files();

  for (i=0;i<numfiles;i++)
    st_free(files[i]);

  st_free(files);

  return success;
}

static bool fix_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
