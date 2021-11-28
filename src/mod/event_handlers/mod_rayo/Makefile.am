include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_rayo

IKS_DIR=$(switch_srcdir)/libs/iksemel
IKS_BUILDDIR=$(switch_builddir)/libs/iksemel
IKS_LA=$(IKS_BUILDDIR)/src/libiksemel.la

noinst_LTLIBRARIES = librayomod.la
librayomod_la_SOURCES = mod_rayo.c iks_helpers.c nlsml.c rayo_components.c rayo_cpa_component.c rayo_cpa_detector.c rayo_elements.c rayo_fax_components.c
librayomod_la_SOURCES += rayo_input_component.c rayo_output_component.c rayo_prompt_component.c rayo_record_component.c sasl.c srgs.c xmpp_streams.c rayo_exec_component.c
librayomod_la_CFLAGS = $(AM_CFLAGS) -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS)

mod_LTLIBRARIES = mod_rayo.la
mod_rayo_la_SOURCES  = 
mod_rayo_la_CFLAGS   = $(AM_CFLAGS) -I$(IKS_DIR)/include $(PCRE_CFLAGS)
mod_rayo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(IKS_LA) $(PCRE_LIBS) librayomod.la
mod_rayo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(IKS_LA)

$(IKS_LA): $(IKS_BUILDDIR) $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_BUILDDIR) && $(MAKE)
	@$(TOUCH_TARGET)

noinst_PROGRAMS = test/test_iks test/test_nlsml test/test_srgs

test_test_iks_SOURCES = test/test_iks.c
test_test_iks_CFLAGS = $(AM_CFLAGS) -I. -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS) -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_iks_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_iks_LDADD = librayomod.la $(IKS_LA) $(PCRE_LIBS) $(switch_builddir)/libfreeswitch.la

test_test_nlsml_SOURCES = test/test_nlsml.c
test_test_nlsml_CFLAGS = $(AM_CFLAGS) -I. -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS) -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_nlsml_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_nlsml_LDADD = librayomod.la $(IKS_LA) $(PCRE_LIBS) $(switch_builddir)/libfreeswitch.la

test_test_srgs_SOURCES = test/test_srgs.c
test_test_srgs_CFLAGS = $(AM_CFLAGS) -I. -I$(switch_builddir)/libs/iksemel/include $(PCRE_CFLAGS) -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_srgs_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_srgs_LDADD = librayomod.la $(IKS_LA) $(PCRE_LIBS) $(switch_builddir)/libfreeswitch.la


TESTS = $(noinst_PROGRAMS)

