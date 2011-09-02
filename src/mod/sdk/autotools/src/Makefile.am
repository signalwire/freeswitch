moddir     = @FREESWITCH_MODULES_DIR@
sysconfdir = @FREESWITCH_CONFIG_DIR@

###
# Flags
#
AM_CFLAGS  =
AM_CXXFLAGS=
AM_CPPFLAGS= -I. -I$(includedir)
AM_LDFLAGS = -L. -L$(libdir) -avoid-version -module -no-undefined -shared

###
# GCC (and compatible) specific flags
#
if CC_VENDOR_GNU
AM_CFLAGS += -Wall
endif

###
# SunCC specific flags
#
if CC_VENDOR_SUN
AM_CFLAGS +=
endif

###
# symbol visibility support
#
if CC_HAS_VISIBILITY
AM_CFLAGS   += $(VISIBILITY_CFLAGS)
AM_CXXFLAGS += $(VISIBILITY_CXXFLAGS)
AM_CPPFLAGS += -DSWITCH_API_VISIBILITY=1
endif


###
# add module(s) here, with suffix '.la'
#
mod_LTLIBRARIES = mod_example.la

###
# mod_example
#
mod_example_la_SOURCES = mod_example.c
mod_example_la_CFLAGS  = $(AM_CFLAGS) $(FREESWITCH_CFLAGS)
mod_example_la_CPPFLAGS= $(AM_CPPFLAGS) $(FREESWITCH_CPPFLAGS)
mod_example_la_LDFLAGS = $(AM_LDFLAGS)
mod_example_la_LIBADD  = $(FREESWITCH_LIBS)

# configuration file
#sysconf_DATA = example.conf.xml

#
# install configuration
#
#install-sysconfDATA:
#	$(INSTALL) -d $(DESTDIR)/$(sysconfdir)
#	for x in $(sysconf_DATA); do \
#		test -e $(DESTDIR)$(sysconfdir)/$$x || $(INSTALL) -m644 $$x $(DESTDIR)$(sysconfdir)/$$x ; \
#	done
