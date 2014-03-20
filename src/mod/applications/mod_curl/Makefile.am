include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS += foreign
MODNAME=mod_curl

JSON_C=json-c-0.9
JSON_DIR=$(switch_srcdir)/libs/$(JSON_C)
JSON_BUILDDIR=$(switch_builddir)/libs/$(JSON_C)
JSONLA=$(JSON_BUILDDIR)/libjson.la

mod_LTLIBRARIES = mod_curl.la
mod_curl_la_SOURCES  = mod_curl.c
mod_curl_la_CFLAGS   = $(AM_CFLAGS) -I$(JSON_DIR)
mod_curl_la_CPPFLAGS = $(AM_CPPFLAGS)
mod_curl_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(JSONLA)
mod_curl_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(JSONLA)

$(JSON_DIR):
	$(GETLIB) $(JSON_C).tar.gz

$(JSON_BUILDDIR)/Makefile: $(JSON_DIR)
	mkdir -p $(JSON_BUILDDIR)
	cd $(JSON_BUILDDIR) && $(DEFAULT_VARS) $(JSON_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(JSON_DIR) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(JSONLA): $(JSON_BUILDDIR)/Makefile
	cd $(JSON_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)



