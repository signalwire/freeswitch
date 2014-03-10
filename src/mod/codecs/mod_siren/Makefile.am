include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_siren
G722_1_DIR=$(switch_srcdir)/libs/libg722_1
G722_1_BUILDDIR=$(switch_builddir)/libs/libg722_1
G722_1_LA=$(G722_1_BUILDDIR)/src/.libs/libg722_1.la
mod_LTLIBRARIES = mod_siren.la
mod_siren_la_SOURCES  = mod_siren.c
mod_siren_la_CFLAGS   = $(AM_CFLAGS) -I$(G722_1_DIR)/src  -I$(G722_1_BUILDDIR)/src
mod_siren_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(G722_1_LA)
mod_siren_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(G722_1_LA)

$(G722_1_LA): $(G722_1_DIR)/.update	
	cd $(G722_1_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
