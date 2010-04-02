include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skypopen
mod_LTLIBRARIES = mod_skypopen.la
mod_skypopen_la_SOURCES  = mod_skypopen.c skypopen_protocol.c
mod_skypopen_la_CFLAGS   = $(AM_CFLAGS) -DSKYPOPEN_SVN_VERSION=\"`git describe`\" -I../../../../libs/spandsp/src -I../../../..//libs/tiff-3.8.2/libtiff
mod_skypopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skypopen_la_LDFLAGS  = -L../../../../libs/spandsp/src -avoid-version -module -no-undefined -shared -lX11 -lspandsp
