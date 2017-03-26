/*  core_shntool.c - functions to handle mode verification and execution
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
#include <signal.h>
#include "shntool.h"

CVSID("$Id: core_shntool.c,v 1.90 2009/03/16 04:46:03 jason Exp $")

private_opts st_priv;
input_files st_input;

void st_version()
{
  st_info(
          "%s%s " RELEASE "\n"
          COPYRIGHT " " AUTHOR "\n"
          "\n"
          "shorten utilities pages:\n"
          "\n"
          "  " URL1 "\n"
          "  " URL2 "\n"
          "\n"
          "This is free software.  You may redistribute copies of it under the terms of\n"
          "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
          "There is NO WARRANTY, to the extent permitted by law.\n"
          "\n"
          ,st_priv.fullprogname,(st_priv.progmode)?" mode module":""
         );
}

static void show_supported_mode_modules()
{
  int i;

  st_info("%s %s supported modes:\n",st_priv.fullprogname,RELEASE);
  st_info("\n");
  st_info("   mode  description\n");
  st_info("   ----  -----------\n");
  for (i=0;st_modes[i];i++)
    st_info("  %5s  %s\n",st_modes[i]->name,st_modes[i]->description);
  st_info("\n");
  st_info("  For help with any of the above modes, type:  '%s mode -h'\n",st_priv.progname);
  st_info("\n");
}

static void show_supported_format_modules()
{
  int i;
  char tmp[BUF_SIZE];

  st_info("%s %s supported file formats:\n",st_priv.fullprogname,RELEASE);
  st_info("\n");

  st_info(" format    ext     input    output  description\n");
  st_info(" ------    ---     -----    ------  -----------\n");
  for (i=0;st_formats[i];i++) {
    st_info("  %5s",st_formats[i]->name);

    if (st_formats[i]->supports_output && st_formats[i]->extension) {
      st_snprintf(tmp,BUF_SIZE,".%s",st_formats[i]->extension);
      st_info("  %5s",tmp);
    }
    else
      st_info("      -");

    if (st_formats[i]->supports_input) {
      if (st_formats[i]->decoder) {
        st_info("  %8s",st_formats[i]->decoder);
      }
      else {
        st_info("   shntool");
      }
    }
    else {
      st_info("         -");
    }

    if (st_formats[i]->supports_output) {
      if (st_formats[i]->encoder) {
        st_info("  %8s",st_formats[i]->encoder);
      }
      else {
        st_info("   shntool");
      }
    }
    else {
      st_info("         -");
    }

    st_info("  %s\n",st_formats[i]->description);
  }

  st_info("\n");
}

static void show_format_module_args()
{
  int i,showed_format;

  st_info("%s %s default file format arguments:\n",st_priv.fullprogname,RELEASE);
  st_info("\n");

  st_info(" format    action   arguments\n");
  st_info(" ------    ------   ---------\n");

  for (i=0;st_formats[i];i++) {

    if (!st_formats[i]->decoder && !st_formats[i]->encoder) {
      continue;
    }

    showed_format = 0;

    if (st_formats[i]->decoder) {
      st_info("  %5s  decoding: ",(showed_format)?"":st_formats[i]->name);
      st_info(" -i \"%s %s %s\"",st_formats[i]->name,st_formats[i]->decoder,st_formats[i]->decoder_args);
      st_info("\n");
      showed_format = 1;
    }

    if (st_formats[i]->encoder) {
      st_info("  %5s  encoding: ",(showed_format)?"":st_formats[i]->name);
      st_info(" -o \"%s ext=%s %s %s\"",st_formats[i]->name,st_formats[i]->extension,st_formats[i]->encoder,st_formats[i]->encoder_args);
      st_info("\n");
      showed_format = 1;
    }

  }

  st_info("\n");
}

static void core_help()
{
  st_info("Usage: %s mode ...\n",st_priv.progname);
  st_info("       %s [OPTIONS]\n",st_priv.progname);
  st_info("\n");
  st_info("Options:\n");
  st_info("\n");
  st_info("  -m      show detailed mode module information\n");
  st_info("  -f      show detailed format module information\n");
  st_info("  -a      show default format module arguments\n");
  st_info("  -v      show version information\n");
  st_info("  -h      show this help screen\n");
  st_info("\n");
}

static void module_sanity_check()
{
  int i,j;

  if (st_modes[0]) {
    for (i=0;st_modes[i];i++) {
      if (NULL == st_modes[i]->name)
        st_error("found a mode module without a name");

      if (NULL == st_modes[i]->description)
        st_error("mode module has no description: [%s]",st_modes[i]->name);

      if (NULL == st_modes[i]->run_main)
        st_error("mode module does not provide run_main(): [%s]",st_modes[i]->name);
    }

    for (i=0;st_modes[i];i++) {
      for (j=i+1;st_modes[j];j++) {
        if (!strcmp(st_modes[i]->name,st_modes[j]->name))
          st_error("found modes with identical name: [%s]",st_modes[i]->name);
      }
    }
  }
  else
    st_error("no mode modules found");

  if (st_formats[0]) {
    for (i=0;st_formats[i];i++) {
      if (NULL == st_formats[i]->name)
        st_error("found a format module without a name");

      if (NULL == st_formats[i]->description)
        st_error("format module has no description: [%s]",st_formats[i]->name);

      if (!st_formats[i]->supports_input && !st_formats[i]->supports_output)
        st_error("format module doesn't support input or output: [%s]",st_formats[i]->name);

      if (!st_formats[i]->supports_input && st_formats[i]->decoder)
        st_error("format module doesn't support input but defines a decoder: [%s]",st_formats[i]->name);

      if (!st_formats[i]->supports_output && st_formats[i]->encoder)
        st_error("format module doesn't support output but defines an encoder: [%s]",st_formats[i]->name);
    }

    for (i=0;st_formats[i];i++) {
      for (j=i+1;st_formats[j];j++) {
        if (!strcmp(st_formats[i]->name,st_formats[j]->name))
          st_error("found formats with identical name: [%s]",st_formats[i]->name);
      }
    }
  }
  else {
    st_error("no file formats modules found");
  }
}

static bool parse_main(int argc,char **argv)
{
  int i,j;
  int c;

  /* look for a module alias matching progname */

  for (i=0;st_modes[i];i++) {
    if (NULL != st_modes[i]->alias && !strcmp(st_priv.progname,st_modes[i]->alias)) {
      st_priv.mode = st_modes[i];
      st_priv.is_aliased = TRUE;
      st_priv.progmode = st_modes[i]->name;
      return st_modes[i]->run_main(argc,argv);
    }
  }

  /* failing that, look for a module name matching argv[1] */

  if (argc < 2)
    st_help("missing arguments");

  for (i=0;st_modes[i];i++) {
    if (!strcmp(argv[1],st_modes[i]->name)) {
      /* found mode - now run it and quit */
      st_priv.mode = st_modes[i];
      st_priv.progmode = st_modes[i]->name;
      strcat(st_priv.fullprogname," ");
      strcat(st_priv.fullprogname,st_priv.progmode);

      /* remove mode name from arg list (is this portable?) */
      for (j=1;j<argc-1;j++)
        argv[j] = argv[j+1];
      argv[argc-1] = NULL;
      argc--;

      return st_modes[i]->run_main(argc,argv);
    }
  }

  while ((c=getopt(argc,argv,GLOBAL_OPTS_CORE)) != -1) {
    switch (c) {
      case 'a':
        show_format_module_args();
        exit(ST_EXIT_SUCCESS);
        break;
      case 'f':
        show_supported_format_modules();
        exit(ST_EXIT_SUCCESS);
        break;
      case 'j':
        st_info("Guru Meditashn #00000002.9091968B\n");
        exit(ST_EXIT_SUCCESS);
        break;
      case 'm':
        show_supported_mode_modules();
        exit(ST_EXIT_SUCCESS);
        break;
      case '?':
      case 'h':
        if ('?' == c)
          st_info("\n");
        core_help();
        exit(('?' == c) ? ST_EXIT_ERROR : ST_EXIT_SUCCESS);
        break;
      case 'v':
        st_version();
        exit(ST_EXIT_SUCCESS);
        break;
    }
  }

  if (optind < argc)
    st_help("invalid mode: [%s]",argv[optind]);

  return FALSE;
}

static void modules_init()
{
  int i;

  /* sanity checks for modules */
  module_sanity_check();

  /* initialize format modules */
  for (i=0;st_formats[i];i++) {
    arg_init(st_formats[i]);
  }
}

static void globals_init(char *program)
{
  char *p;
  int n;

#ifdef WIN32
  setvbuf(stdout,NULL,_IONBF,0);
#else
  signal(SIGPIPE,SIG_IGN);
#endif

  /* public globals */
  st_ops.output_directory = ".";
  st_ops.output_prefix = "";
  st_ops.output_postfix = "";
  st_ops.output_format = NULL;

  /* private globals */
  st_priv.progname = ((p = strrchr(program,PATHSEPCHAR))) ? (p + 1) : program;
  if ((p = extname(st_priv.progname)))
    *(p-1) = 0;
  strcpy(st_priv.fullprogname,st_priv.progname);
  st_priv.progmode = NULL;
  st_priv.clobber_action = CLOBBER_ACTION_ASK;
  st_priv.reorder_type = ORDER_NATURAL;
  st_priv.progress_type = PROGRESS_PERCENT;
  st_priv.is_aliased = FALSE;
  st_priv.show_hmmss = FALSE;
  st_priv.suppress_warnings = FALSE;
  st_priv.suppress_stderr = FALSE;
  st_priv.screen_dirty = FALSE;

  st_input.type = INPUT_CMDLINE;
  st_input.filename_source = NULLDEVICE;
  st_input.fd = NULL;
  st_input.argn = 0;
  st_input.argc = 0;
  st_input.argv = NULL;
  st_input.filecur = 0;
  st_input.filemax = 0;

  p = scan_env(SHNTOOL_DEBUG_ENV);
  n = p ? atoi(p) : 0;

  st_priv.debug_level = (n > 0) ? n : 0;
}

int main(int argc,char **argv)
{
  bool success;

  /* initialize global variables */
  globals_init(argv[0]);

  /* initialize modules */
  modules_init();

  /* parse command line */
  success = parse_main(argc,argv);

  return (success) ? ST_EXIT_SUCCESS : ST_EXIT_ERROR;
}
