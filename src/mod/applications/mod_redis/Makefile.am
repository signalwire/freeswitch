include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_redis

mod_LTLIBRARIES = mod_redis.la
mod_redis_la_SOURCES  = mod_redis.c credis.c
mod_redis_la_CFLAGS   = $(AM_CFLAGS) -D_GNU_SOURCE
mod_redis_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_redis_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
