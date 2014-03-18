include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_esl
ESL_DIR = $(switch_srcdir)/libs/esl

ESL_SRCS  =../../../../libs/esl/src/esl.c ../../../../libs/esl/src/esl_json.c
ESL_SRCS +=../../../../libs/esl/src/esl_event.c ../../../../libs/esl/src/esl_threadmutex.c
ESL_SRCS +=../../../../libs/esl/src/esl_config.c ../../../../libs/esl/src/esl_oop.cpp
ESL_SRCS +=../../../../libs/esl/src/esl_buffer.c

mod_LTLIBRARIES = mod_esl.la
mod_esl_la_SOURCES  = mod_esl.c $(ESL_SRCS)
mod_esl_la_CFLAGS   = $(AM_CFLAGS)
mod_esl_la_CPPFLAGS = -I$(ESL_DIR)/src/include
mod_esl_la_LIBADD   = $(switch_builddir)/libfreeswitch.la #$(ESL_DIR)/libesl.a
mod_esl_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

#$(ESL_DIR)/libesl.a: $(ESL_DIR)/Makefile
#	cd $(ESL_DIR) && $(MAKE) -j1
