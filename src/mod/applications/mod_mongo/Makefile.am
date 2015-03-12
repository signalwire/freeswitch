include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS += foreign
MODNAME=mod_mongo

MONGOC=mongo-c-driver-1.1.0
MONGOC_DIR=$(switch_srcdir)/libs/$(MONGOC)
MONGOC_BUILDDIR=$(switch_builddir)/libs/$(MONGOC)
MONGOC_LA=$(MONGOC_BUILDDIR)/libmongoc-priv.la

mod_LTLIBRARIES = mod_mongo.la
mod_mongo_la_SOURCES  = mod_mongo.c
mod_mongo_la_CFLAGS   = $(AM_CFLAGS) -I$(MONGOC_DIR)/src/mongoc/ -I$(MONGOC_DIR)/src/libbson/src/bson/
mod_mongo_la_CPPFLAGS = $(AM_CPPFLAGS) 
mod_mongo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MONGOC_LA)
mod_mongo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared 

BUILT_SOURCES=$(MONGOC_LA)

$(MONGOC_DIR)/configure:
	$(GETLIB) $(MONGOC).tar.gz
	sed -i'' -e 's/cruT/cru/' $(MONGOC_DIR)/configure
	sed -i'' -e 's/cruT/cru/' $(MONGOC_DIR)/build/autotools/SetupLibtool.m4
	sed -i'' -e 's/cruT/cru/' $(MONGOC_DIR)/src/libbson/configure
	sed -i'' -e 's/cruT/cru/' $(MONGOC_DIR)/src/libbson/build/autotools/SetupLibtool.m4
	cd $(MONGOC_DIR) && autoreconf -i
	$(TOUCH_TARGET)

$(MONGOC_BUILDDIR)/Makefile: $(MONGOC_DIR)/configure
	mkdir -p $(MONGOC_BUILDDIR)
	cd $(MONGOC_BUILDDIR) && $(DEFAULT_VARS) $(MONGOC_DIR)/configure $(DEFAULT_ARGS)
	$(TOUCH_TARGET)

$(MONGOC_LA): $(MONGOC_BUILDDIR)/Makefile
	cd $(MONGOC_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

