dnl Process this file with autoconf to produce a configure script.

AC_INIT([shntool],[3.0.10],[shnutils@freeshell.org])
AC_REVISION([$Id: configure.ac,v 1.125 2009/03/30 05:55:33 jason Exp $])

AC_CONFIG_SRCDIR([src/core_shntool.c])
AC_CONFIG_HEADERS([include/config.h])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_LANG([C])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl Checks for types.
AC_C_BIGENDIAN

dnl configure command-line options

dnl default modes - used if --with-modes is not specified
ac_build_modes="len fix hash pad join split cat cmp cue conv info strip gen trim"

dnl default file formats - used if --with-formats is not specified
ac_build_formats="wav aiff shn flac ape alac tak ofr tta als wv lpac la mkw bonk kxs cust term null"

ac_alternate_modes=
ac_alternate_formats=

ac_alternate_modes_flag=false
ac_alternate_formats_flag=false

dnl mode module specification
AC_ARG_WITH([modes],
  [  --with-modes=LIST           Specify default modes],
  [ ac_alternate_modes_flag=true ; ac_alternate_modes="`echo "$withval" | sed -e 's/,/ /g'`" ]
)

dnl format module specification
AC_ARG_WITH([formats],
  [  --with-formats=LIST         Specify default file formats],
  [ ac_alternate_formats_flag="true" ; ac_alternate_formats="`echo "$withval" | sed -e 's/,/ /g'`"]
)

if test "x$ac_alternate_modes_flag" = "xfalse"; then
  ac_current_modes="$ac_build_modes"
else
  ac_current_modes="$ac_alternate_modes"
fi

if test "x$ac_alternate_formats_flag" = "xfalse"; then
  ac_current_formats="$ac_build_formats"
else
  ac_current_formats="$ac_alternate_formats"
fi

dnl are additional modules specified?

AC_ARG_WITH([extra-modes],
  [  --with-extra-modes=LIST     Specify additional modes],
  [ ac_more_modes="`echo "$withval" | sed -e 's/,/ /g'`" ; ac_current_modes="$ac_current_modes $ac_more_modes" ]
)

AC_ARG_WITH([extra-formats],
  [  --with-extra-formats=LIST   Specify additional file formats],
  [ ac_more_formats="`echo "$withval" | sed -e 's/,/ /g'`" ; ac_current_formats="$ac_current_formats $ac_more_formats" ]
)

ac_list_modes=
ac_list_formats=
mode_exists=false
format_exists=false

MODE_OBJS=
FORMAT_OBJS=
MODES_CONFIGURED=

for mode in $ac_current_modes; do
  if test "`echo "$ac_list_modes" | grep -c "mode_${mode}\.c"`" = "0"; then
    if test -f ${srcdir}/src/mode_${mode}.c; then
      ac_list_modes="${ac_list_modes}mode_${mode}.c "
      MODE_OBJS="${MODE_OBJS}mode_${mode}.\$(OBJEXT) "
      MODES_CONFIGURED="${MODES_CONFIGURED}${mode} "
      mode_exists=true
    else
      AC_MSG_WARN([missing ${srcdir}/src/mode_${mode}.c -- mode '${mode}' will not be compiled])
    fi
  fi
done

for format in $ac_current_formats; do
  if test "`echo "$ac_list_formats" | grep -c "format_${format}\.c"`" = "0"; then
    if test -f ${srcdir}/src/format_${format}.c; then
      ac_list_formats="${ac_list_formats}format_${format}.c "
      FORMAT_OBJS="${FORMAT_OBJS}format_${format}.\$(OBJEXT) "
      format_exists=true
      case $format in
        aiff ) format_aiff=true ;;
        ape  ) format_ape=true ;;
        flac ) format_flac=true ;;
        lpac ) format_lpac=true ;;
        ofr  ) format_ofr=true ;;
        shn  ) format_shn=true ;;
        wv   ) format_wv=true ;;
        alac ) format_alac=true ;;
        la   ) format_la=true ;;
        tta  ) format_tta=true ;;
        als  ) format_als=true ;;
        tak  ) format_tak=true ;;
        bonk ) format_bonk=true ;;
        kxs  ) format_kxs=true ;;
        mkw  ) format_mkw=true ;;
      esac
    else
      AC_MSG_WARN([missing ${srcdir}/src/format_${format}.c -- format '${format}' will not be compiled])
    fi
  fi
done

if test x$mode_exists = xfalse; then
  AC_MSG_ERROR([at least one valid mode must be specified],[1])
fi

if test x$format_exists = xfalse; then
  AC_MSG_ERROR([at least one valid file format must be specified],[1])
fi

AC_SUBST([MODE_OBJS])
AC_SUBST([FORMAT_OBJS])
AC_SUBST([MODES_CONFIGURED])

dnl Additional program checks, based on whether certain file formats are included in shntool

echo
AC_MSG_NOTICE([checking for optional helper programs])
echo

if test x$format_shn = xtrue; then
  AC_CHECK_PROG([SHORTEN],[shorten],[yes],[no (install for shn support)])
fi

if test x$format_flac = xtrue; then
  AC_CHECK_PROG([FLAC],[flac],[yes],[no (install for flac support)])
fi

if test x$format_aiff = xtrue; then
  AC_CHECK_PROG([SOX],[sox],[yes],[no (install for aiff support)])
fi

if test x$format_ape = xtrue; then
  AC_CHECK_PROG([MAC],[mac],[yes],[no (install for ape support)])
fi

if test x$format_ofr = xtrue; then
  AC_CHECK_PROG([OPTIMFROG],[ofr],[yes],[no (install for ofr support)])
fi

if test x$format_lpac = xtrue; then
  AC_CHECK_PROG([LPAC],[lpac],[yes],[no (install for lpac support)])
fi

if test x$format_wv = xtrue; then
  AC_CHECK_PROG([WAVPACK],[wvunpack],[yes],[no (install for wv support)])
fi

if test x$format_alac = xtrue; then
  AC_CHECK_PROG([ALAC],[alac],[yes],[no (install for alac support)])
fi

if test x$format_la = xtrue; then
  AC_CHECK_PROG([LA],[la],[yes],[no (install for la support)])
fi

if test x$format_tta = xtrue; then
  AC_CHECK_PROG([TTA],[ttaenc],[yes],[no (install for tta support)])
fi

if test x$format_als = xtrue; then
  AC_CHECK_PROG([ALS],[mp4als],[yes],[no (install for als support)])
fi

if test x$format_mkw = xtrue; then
  AC_CHECK_PROG([MKWCON],[mkwcon],[yes],[no (install for mkw support)])
fi

if test x$format_tak = xtrue; then
  AC_CHECK_PROG([TAKC],[takc],[yes],[no (install for tak support)])
fi

if test x$format_bonk = xtrue; then
  AC_CHECK_PROG([BONK],[bonk],[yes],[no (install for bonk support)])
fi

if test x$format_kxs = xtrue; then
  AC_CHECK_PROG([KEXIS],[kexis],[yes],[no (install for kxs support)])
fi

dnl Checks for header files.
echo
AC_MSG_NOTICE([checking for win32 environment])
echo
AC_CHECK_HEADERS([windows.h])

dnl Checks for library functions.
echo
AC_MSG_NOTICE([checking for library functions])
echo
AC_CHECK_FUNCS([strerror vsnprintf atol sysconf])

echo
AC_MSG_NOTICE([creating build files])
echo
AC_OUTPUT([man/Makefile src/Makefile Makefile])

ac_compiled_modes="`echo "$ac_list_modes" | sed -e 's/mode_//g' -e 's/\.c//g'`"
ac_compiled_formats="`echo "$ac_list_formats" | sed -e 's/format_//g' -e 's/\.c//g'`"

echo
AC_MSG_NOTICE([creating module glue files])
echo
echo "processing modes..."
sh ${srcdir}/utils/glue.sh mode   $ac_compiled_modes   > src/glue_modes.c
echo "processing formats..."
sh ${srcdir}/utils/glue.sh format $ac_compiled_formats > src/glue_formats.c

echo
AC_MSG_NOTICE([ready to compile.

$PACKAGE is now configured with the following options:

version :  $VERSION
modes   :  $ac_compiled_modes
formats :  $ac_compiled_formats
install :  ${prefix}/bin
])
