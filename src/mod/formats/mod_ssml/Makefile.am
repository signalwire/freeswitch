include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ssml

IKS_DIR=$(switch_srcdir)/libs/iksemel
IKS_BUILDDIR=$(switch_builddir)/libs/iksemel
IKS_LA=$(IKS_BUILDDIR)/src/libiksemel.la

mod_LTLIBRARIES = mod_ssml.la
mod_ssml_la_SOURCES  = mod_ssml.c
mod_ssml_la_CFLAGS   = $(AM_CFLAGS) -I$(IKS_DIR)/include
mod_ssml_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(IKS_LA)
mod_ssml_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(IKS_LA)

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)
