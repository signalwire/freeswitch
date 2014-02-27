include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_g723_1

DIR=$(switch_srcdir)/libs/libg723_1
BUILDDIR=$(switch_builddir)/libs/libg723_1
G723_1_A=$(BUILDDIR)/.libs/libg723.a

mod_LTLIBRARIES = mod_g723_1.la
mod_g723_1_la_SOURCES  = mod_g723_1.c
mod_g723_1_la_CFLAGS   = $(AM_CFLAGS)
mod_g723_1_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_g723_1_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_G723_1
mod_g723_1_la_CFLAGS += -I$(DIR)/src/include
mod_g723_1_la_LIBADD += $(G723_1_A)
else
mod_g723_1_la_CFLAGS += -DG723_PASSTHROUGH
endif
