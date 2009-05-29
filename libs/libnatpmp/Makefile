# $Id: Makefile,v 1.7 2009/01/23 19:05:04 nanard Exp $
# This Makefile is designed for use with GNU make
# libnatpmp
# (c) 2007-2009 Thomas Bernard
# http://miniupnp.free.fr/libnatpmp.html

CC = gcc
INSTALL = install

# APIVERSION is used in soname
APIVERSION = 0
CFLAGS = -O -fPIC -Wall -DENABLE_STRNATPMPERR

LIBOBJS = natpmp.o getgateway.o

OBJS = $(LIBOBJS) testgetgateway.o natpmpc.o

STATICLIB = libnatpmp.a
SHAREDLIB = libnatpmp.so
SONAME = $(SHAREDLIB).$(APIVERSION)

HEADERS = natpmp.h

EXECUTABLES = testgetgateway natpmpc-shared natpmpc-static

INSTALLPREFIX ?= $(PREFIX)/usr
INSTALLDIRINC = $(INSTALLPREFIX)/include
INSTALLDIRLIB = $(INSTALLPREFIX)/lib
INSTALLDIRBIN = $(INSTALLPREFIX)/bin

.PHONY:	all clean depend install cleaninstall

all: $(STATICLIB) $(SHAREDLIB) $(EXECUTABLES)

clean:
	$(RM) $(OBJS) $(EXECUTABLES) $(STATICLIB) $(SHAREDLIB)

depend:
	makedepend -f$(MAKEFILE_LIST) -Y $(OBJS:.o=.c) 2>/dev/null

install:	$(HEADERS) $(STATICLIB) $(SHAREDLIB)
	$(INSTALL) -d $(INSTALLDIRINC)
	$(INSTALL) --mode=644 $(HEADERS) $(INSTALLDIRINC)
	$(INSTALL) -d $(INSTALLDIRLIB)
	$(INSTALL) --mode=644 $(STATICLIB) $(INSTALLDIRLIB)
	$(INSTALL) --mode=644 $(SHAREDLIB) $(INSTALLDIRLIB)/$(SONAME)
	$(INSTALL) --mode=755 natpmpc-shared $(INSTALLDIRBIN)/natpmpc
	ln --symbolic --force $(SONAME) $(INSTALLDIRLIB)/$(SHAREDLIB)

cleaninstall:
	$(RM) $(addprefix $(INSTALLDIRINC), $(HEADERS))
	$(RM) $(INSTALLDIRLIB)/$(SONAME)
	$(RM) $(INSTALLDIRLIB)/$(SHAREDLIB)
	$(RM) $(INSTALLDIRLIB)/$(STATICLIB)

testgetgateway:	testgetgateway.o getgateway.o

natpmpc-static:	natpmpc.o $(STATICLIB)
	$(CC) -o $@ $^

natpmpc-shared:	natpmpc.o $(SHAREDLIB)
	$(CC) -o $@ $^

$(STATICLIB):	$(LIBOBJS)
	$(AR) crs $@ $?

$(SHAREDLIB):	$(LIBOBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^

# DO NOT DELETE

natpmp.o: natpmp.h getgateway.h
getgateway.o: getgateway.h
testgetgateway.o: getgateway.h
natpmpc.o: natpmp.h
