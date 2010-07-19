include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_hash
ESL_DIR=$(switch_srcdir)/libs/esl

mod_LTLIBRARIES = mod_hash.la
mod_hash_la_SOURCES  = mod_hash.c
mod_hash_la_CFLAGS   = $(AM_CFLAGS) -I$(ESL_DIR)/src/include
mod_hash_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(ESL_DIR)/libesl.so
mod_hash_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -L$(ESL_DIR) -lesl

$(ESL_DIR)/libesl.so:
	cd $(ESL_DIR) && $(MAKE) -j1
