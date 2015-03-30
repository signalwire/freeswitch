include $(top_srcdir)/build/modmake.rulesam

AUTOMAKE_OPTIONS += foreign
MODNAME=mod_mongo

if HAVE_MONGOC

mod_LTLIBRARIES = mod_mongo.la
mod_mongo_la_SOURCES  = mod_mongo.c
mod_mongo_la_CFLAGS   = $(AM_CFLAGS) $(MONGOC_CFLAGS)
mod_mongo_la_CPPFLAGS = $(AM_CPPFLAGS) 
mod_mongo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(MONGOC_LIBS)
mod_mongo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared 


else
install: error
all: error
error:
	$(error You must install libmongoc-dev to build mod_mongo)
endif
