include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cdr_mongodb
MONGODB_DRIVER=./driver/src
mod_LTLIBRARIES = mod_cdr_mongodb.la
mod_cdr_mongodb_la_SOURCES  = mod_cdr_mongodb.c $(MONGODB_DRIVER)/encoding.c $(MONGODB_DRIVER)/env_posix.c $(MONGODB_DRIVER)/bson.c $(MONGODB_DRIVER)/md5.c $(MONGODB_DRIVER)/mongo.c $(MONGODB_DRIVER)/numbers.c
mod_cdr_mongodb_la_CFLAGS   = $(AM_CFLAGS)
mod_cdr_mongodb_la_CPPFLAGS = -I$(MONGODB_DRIVER) -I$(switch_srcdir)/libs/stfu/ -I$(switch_srcdir)/libs/libteletone/src/ -D_GNU_SOURCE                          
mod_cdr_mongodb_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cdr_mongodb_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
