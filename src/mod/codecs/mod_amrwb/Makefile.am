include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_amrwb

mod_LTLIBRARIES = mod_amrwb.la
if HAVE_AMRWB
mod_amrwb_la_SOURCES  = mod_amrwb.c bitshift.c amrwb_be.c
else
mod_amrwb_la_SOURCES  = mod_amrwb.c 
endif

mod_amrwb_la_CFLAGS   = $(AM_CFLAGS)
mod_amrwb_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_amrwb_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_AMRWB
# install AMRWB libraries:  opencore-amrwb (decoder) and vo-amrwbenc (encoder)  - check README
mod_amrwb_la_CFLAGS += $(AMRWB_CFLAGS) 
mod_amrwb_la_LIBADD += $(AMRWB_LIBS) 
else
mod_amrwb_la_CFLAGS += -DAMRWB_PASSTHROUGH
endif

if HAVE_AMRWB
noinst_PROGRAMS = test/test_amrwb

test_test_amrwb_SOURCES = test/test_amrwb.c
test_test_amrwb_CFLAGS = $(AM_CFLAGS) -I./ -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\" $(AMRWB_CFLAGS)
test_test_amrwb_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS) $(AMRWB_LIBS)

TESTS = $(noinst_PROGRAMS)
endif

