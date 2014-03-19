include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_h323

mod_LTLIBRARIES = mod_h323.la
mod_h323_la_SOURCES  = mod_h323.cpp
mod_h323_la_CPPFLAGS = $(AM_CPPFLAGS) -I/usr/include/openh323  -I. -DPTRACING=1 -D_REENTRANT
mod_h323_la_CPPFLAGS += -DPTRACING=1 -D_REENTRANT
mod_h323_la_CXXFLAGS = $(AM_CXXFLAGS) -fno-exceptions
mod_h323_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_h323_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -L/usr/lib -lopenh323 -lpt -lrt 

if IS64BITLINUX
mod_h323_la_CPPFLAGS += -DP_64BIT
endif
