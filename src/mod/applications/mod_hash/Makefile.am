include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_hash
ESL_DIR=$(switch_srcdir)/libs/esl

mod_LTLIBRARIES = mod_hash.la
mod_hash_la_SOURCES  = mod_hash.c ../../../../libs/esl/src/esl.c ../../../../libs/esl/src/esl_json.c ../../../../libs/esl/src/esl_event.c ../../../../libs/esl/src/esl_threadmutex.c ../../../../libs/esl/src/esl_config.c ../../../../libs/esl/src/esl_buffer.c
mod_hash_la_CFLAGS   = $(AM_CFLAGS) -I$(ESL_DIR)/src/include
mod_hash_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_hash_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
