include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_gsmopen
mod_LTLIBRARIES = mod_gsmopen.la
mod_gsmopen_la_SOURCES  = mod_gsmopen.cpp gsmopen_protocol.cpp 
mod_gsmopen_la_CFLAGS   = $(AM_CFLAGS) -DGSMOPEN_SVN_VERSION=\"`cat $(switch_builddir)/.version`\"
mod_gsmopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_gsmopen_la_LDFLAGS  = -avoid-version -module -no-undefined -lasound -lgsmme -lspandsp
