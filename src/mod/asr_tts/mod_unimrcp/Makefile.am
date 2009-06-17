MODNAME=mod_unimrcp

AM_CFLAGS   = $(SWITCH_AM_CFLAGS)
AM_CPPFLAGS = $(SWITCH_AM_CXXFLAGS)
#AM_LDFLAGS  = $(SWITCH_AM_LDFLAGS)
#we should set all these vars from configure, no reason to have these in each Makefile.am
LIBTOOL = echo "`link=\`echo $@|grep .la;echo $@|grep .so;echo $@|grep .dll\`;if test -n "$$link"; then echo Creating $@;fi`";`if test -z "$(VERBOSE)" ; \
then echo $(SHELL) $(switch_builddir)/quiet_libtool ;else echo $(SHELL) $(switch_builddir)/libtool;  fi`
AM_MAKEFLAGS=`test -n "$(VERBOSE)" || echo -s`
# Dirty trick to override the link output
LIBS+=> $(MODNAME).log || error="yes";if test -n "$(VERBOSE)" -o "$$error" = "yes";then cat $(MODNAME).log;fi;if test "$$error" = "yes";then exit 1;fi

moddir=@modinstdir@

UNIMRCP_DIR=$(switch_builddir)/libs/unimrcp
UNIMRCPLA=$(UNIMRCP_DIR)/platforms/libunimrcp-client/libunimrcpclient.la

mod_LTLIBRARIES = mod_unimrcp.la
mod_unimrcp_la_SOURCES = mod_unimrcp.c
mod_unimrcp_la_CFLAGS  = $(AM_CFLAGS) 
mod_unimrcp_la_CFLAGS += -I.
mod_unimrcp_la_CFLAGS += -I$(switch_srcdir)/libs/apr/include
mod_unimrcp_la_CFLAGS += -I$(switch_srcdir)/libs/apr-util/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/platforms/libunimrcp-client/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/modules/mrcp-sofiasip/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/modules/mrcp-unirtsp/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp-client/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp-signaling/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcpv2-transport/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/message/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/control/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mrcp/resources/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/mpf/include
mod_unimrcp_la_CFLAGS += -I$(UNIMRCP_DIR)/libs/apr-toolkit/include

mod_unimrcp_la_LIBADD = $(switch_builddir)/libfreeswitch.la $(UNIMRCPLA)
mod_unimrcp_la_LDFLAGS = -avoid-version -module -no-undefined

if ISMAC
mod_unimrcp_la_LDFLAGS += -framework CoreFoundation -framework SystemConfiguration
endif

BUILT_SOURCES = $(UNIMRCPLA)

$(UNIMRCPLA): $(UNIMRCP_DIR) $(UNIMRCP_DIR)/.update
	cd $(UNIMRCP_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(mod_unimrcp_la_SOURCES) : $(BUILT_SOURCES)
