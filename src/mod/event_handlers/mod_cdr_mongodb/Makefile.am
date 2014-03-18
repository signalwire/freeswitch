include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cdr_mongodb
MONGODB_DRIVER=$(switch_srcdir)/src/mod/event_handlers/mod_cdr_mongodb/driver/src
mod_LTLIBRARIES = mod_cdr_mongodb.la
mod_cdr_mongodb_la_SOURCES  = mod_cdr_mongodb.c ./driver/src/encoding.c ./driver/src/env_posix.c ./driver/src/bson.c ./driver/src/md5.c ./driver/src/mongo.c ./driver/src/numbers.c
mod_cdr_mongodb_la_CFLAGS   = $(AM_CFLAGS)
mod_cdr_mongodb_la_CPPFLAGS = -I$(MONGODB_DRIVER) -I$(switch_srcdir)/libs/libteletone/src/ -D_GNU_SOURCE                          
mod_cdr_mongodb_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cdr_mongodb_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
