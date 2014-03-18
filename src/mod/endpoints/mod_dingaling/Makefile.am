include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dingaling
IKS_DIR=$(switch_srcdir)/libs/iksemel
IKS_BUILDDIR=$(switch_builddir)/libs/iksemel
IKS_LA=$(IKS_BUILDDIR)/src/libiksemel.la
DING_DIR=$(switch_srcdir)/libs/libdingaling
DING_BUILDDIR=$(switch_builddir)/libs/libdingaling

mod_LTLIBRARIES = mod_dingaling.la
mod_dingaling_la_SOURCES  = mod_dingaling.c ../../../../libs/libdingaling/src/libdingaling.c ../../../../libs/libdingaling/src/sha1.c
mod_dingaling_la_CFLAGS   = $(AM_CFLAGS)
mod_dingaling_la_CFLAGS  += `$(switch_builddir)/libs/apr/apr-1-config --cflags --cppflags --includes`
mod_dingaling_la_CFLAGS  += `$(switch_builddir)/libs/apr-util/apu-1-config --includes` 
mod_dingaling_la_CFLAGS  += -I$(DING_DIR)/src -I$(DING_BUILDDIR)/src -I$(IKS_DIR)/include
mod_dingaling_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(IKS_LA)
mod_dingaling_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(IKS_LA)

$(IKS_LA): $(IKS_BUILDDIR) $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_BUILDDIR) && $(MAKE)
	@$(TOUCH_TARGET)
