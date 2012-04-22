include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_osp

mod_LTLIBRARIES = mod_osp.la
mod_osp_la_SOURCES  = mod_osp.c 
mod_osp_la_CFLAGS   = $(AM_CFLAGS)
mod_osp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -losptk -lssl -lcrypto -lpthread -lm

