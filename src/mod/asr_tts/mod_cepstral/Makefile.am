include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cepstral

mod_LTLIBRARIES = mod_cepstral.la
mod_cepstral_la_SOURCES  = mod_cepstral.c
mod_cepstral_la_CFLAGS   = $(AM_CFLAGS) -w
mod_cepstral_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cepstral_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if ISMAC
mod_cepstral_la_LDFLAGS += -framework swift
mod_cepstral_la_CFLAGS  += -I/Library/Frameworks/swift.framework/Versions/5/Headers/
else
mod_cepstral_la_LDFLAGS += -Wl,-rpath -Wl,/opt/swift/lib -L$(SWIFT_HOME)/lib -L/opt/swift/lib -lswift -lm
mod_cepstral_la_CFLAGS  += -I$(SWIFT_HOME)/include -I/opt/swift/include 
endif

