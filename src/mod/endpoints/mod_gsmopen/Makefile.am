include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_gsmopen

mod_LTLIBRARIES = mod_gsmopen.la
mod_gsmopen_la_SOURCES  = mod_gsmopen.cpp gsmopen_protocol.cpp 
mod_gsmopen_la_CXXFLAGS = $(SWITCH_AM_CXXFLAGS)
mod_gsmopen_la_CPPFLAGS = $(SPANDSP_CFLAGS) -I.
mod_gsmopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LIBS)
mod_gsmopen_la_LDFLAGS  = -avoid-version -module -no-undefined -lctb-0.16 -lgsmme
