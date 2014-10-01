include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_g729

G729_DIR=$(switch_srcdir)/libs/libg729
G729_BUILDDIR=$(switch_builddir)/libs/libg729
G729_A=$(G729_BUILDDIR)/.libs/libg729.a

mod_LTLIBRARIES = mod_g729.la
mod_g729_la_SOURCES  = mod_g729.c
mod_g729_la_CFLAGS   = $(AM_CFLAGS)
mod_g729_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_g729_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_G729
mod_g729_la_CFLAGS += -I$(G729_DIR)/src/include
mod_g729_la_LIBADD += $(G729_A)
else
mod_g729_la_CFLAGS += -DG729_PASSTHROUGH
endif
