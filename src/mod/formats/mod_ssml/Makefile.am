include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ssml

IKS_DIR=$(switch_srcdir)/libs/iksemel
IKS_BUILDDIR=$(switch_builddir)/libs/iksemel
IKS_LA=$(IKS_BUILDDIR)/src/libiksemel.la

noinst_LTLIBRARIES = libssmlmod.la
libssmlmod_la_SOURCES  = mod_ssml.c
libssmlmod_la_CFLAGS   = $(AM_CFLAGS) -I$(IKS_DIR)/include

mod_LTLIBRARIES = mod_ssml.la
mod_ssml_la_SOURCES  = mod_ssml.c
mod_ssml_la_CFLAGS   = $(AM_CFLAGS) -I$(IKS_DIR)/include
mod_ssml_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(IKS_LA)
mod_ssml_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

noinst_PROGRAMS = test/test_tts_format

test_test_tts_format_SOURCES = test/test_tts_format.c
test_test_tts_format_CFLAGS = $(AM_CFLAGS) -I. -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_tts_format_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_tts_format_LDADD = libssmlmod.la $(IKS_LA)

TESTS = $(noinst_PROGRAMS)

BUILT_SOURCES=$(IKS_LA)

$(IKS_LA): $(IKS_BUILDDIR) $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

deps: $(IKS_LA)
