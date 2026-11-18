include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amr

mod_LTLIBRARIES = mod_amr.la
if HAVE_AMR
mod_amr_la_SOURCES  = mod_amr.c bitshift.c amr_be.c
else
mod_amr_la_SOURCES  = mod_amr.c 
endif

mod_amr_la_CFLAGS   = $(AM_CFLAGS)
mod_amr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_AMR
# install AMR library opencore-amrnb (check README)
mod_amr_la_CFLAGS += $(AMR_CFLAGS) 
mod_amr_la_LIBADD += $(AMR_LIBS) 
else
mod_amr_la_CFLAGS += -DAMR_PASSTHROUGH
endif

if HAVE_AMR
noinst_PROGRAMS = test/test_amr

test_test_amr_SOURCES = test/test_amr.c
test_test_amr_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\" $(AMR_CFLAGS)
test_test_amr_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) $(AMR_LIBS)

TESTS = $(noinst_PROGRAMS)
endif


