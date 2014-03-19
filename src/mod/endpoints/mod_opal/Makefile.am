include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_opal

PKG_DIR=/usr/local/lib/pkgconfig

#DEBUG_SUFFIX:=--define-variable=suffix=_d
mod_LTLIBRARIES = mod_opal.la
mod_opal_la_SOURCES  = mod_opal.cpp
mod_opal_la_CPPFLAGS   = $(AM_CPPFLAGS) $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):$(PKG_DIR) pkg-config opal $(DEBUG_SUFFIX) --cflags | sed 's/-[^ID][^ ]*//g')
mod_opal_la_CXXFLAGS   = $(AM_CXXFLAGS) $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH):$(PKG_DIR) pkg-config opal $(DEBUG_SUFFIX) --cflags | sed 's/-[ID][^ ]*//g')
mod_opal_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_opal_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(shell pkg-config opal $(DEBUG_SUFFIX) --libs)
