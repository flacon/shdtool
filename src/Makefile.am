CORE_SOURCES = core_convert.c core_fileio.c core_format.c core_mode.c core_module.c core_output.c core_shntool.c core_wave.c
GLUE_SOURCES = glue_modes.c glue_formats.c
MODE_SOURCES_ALL = mode_cat.c mode_cmp.c mode_conv.c mode_cue.c mode_fix.c mode_gen.c mode_hash.c mode_info.c mode_join.c mode_len.c mode_pad.c mode_split.c mode_strip.c mode_trim.c
FORMAT_SOURCES_ALL = format_aiff.c format_alac.c format_als.c format_ape.c format_bonk.c format_cust.c format_flac.c format_kxs.c format_la.c format_lpac.c format_mkw.c format_null.c format_ofr.c format_shn.c format_tak.c format_term.c format_tta.c format_wav.c format_wv.c

MODE_ALIASES_ALL = $(shell echo $(MODE_SOURCES_ALL) | sed -e 's/mode_//g' -e 's/\.c//g')
MODE_ALIASES = @MODES_CONFIGURED@

MODULE_OBJS = @MODE_OBJS@ @FORMAT_OBJS@

bin_PROGRAMS = shntool

shntool_SOURCES = $(CORE_SOURCES)
EXTRA_shntool_SOURCES = $(MODE_SOURCES_ALL) $(FORMAT_SOURCES_ALL)
nodist_shntool_SOURCES = $(GLUE_SOURCES)

shntool_LDADD = $(MODULE_OBJS)
shntool_DEPENDENCIES = $(MODULE_OBJS)

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -I$(top_srcdir)/include

install-exec-hook:
	for mode_alias in $(MODE_ALIASES_ALL) ; do \
	  test -e $(DESTDIR)/$(bindir)/shn$$mode_alias && $(RM) $(DESTDIR)/$(bindir)/shn$$mode_alias ; \
	done ; \
	for mode_alias in $(MODE_ALIASES) ; do \
	  test -e $(DESTDIR)/$(bindir)/shn$$mode_alias || $(LN_S) $(PACKAGE) $(DESTDIR)/$(bindir)/shn$$mode_alias ; \
	done

distclean-local:
	$(RM) $(GLUE_SOURCES)
