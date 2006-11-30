#! /usr/bin/make -rf
# $Id: Makefile,v 1.42 2006/11/29 21:27:01 mjt Exp $
# libudns Makefile
#
# Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
# This file is part of UDNS library, an async DNS stub resolver.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library, in file named COPYING.LGPL; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place,
# Suite 330, Boston, MA  02111-1307  USA

VERS   = 0.0.9pre
SRCS   = udns_dn.c udns_dntosp.c udns_parse.c udns_resolver.c udns_misc.c \
	udns_rr_a.c udns_rr_ptr.c udns_rr_mx.c udns_rr_txt.c udns_bl.c \
	udns_rr_srv.c udns_rr_naptr.c udns_codes.c
USRCS  = dnsget.c rblcheck.c ex-rdns.c
DEB    = debian/copyright debian/changelog debian/control debian/rules
DIST   = COPYING.LGPL udns.h udns.3 dnsget.1 rblcheck.1 $(SRCS) $(USRCS) \
	Makefile TODO NOTES

OBJS   = $(SRCS:.c=.o) $(GEN:.c=.o)
LIB    = libudns.a

SOVER  = 0
SOBJS  = $(OBJS:.o=.lo)
SOLIB  = libudns.so
SOLIBV = $(SOLIB).$(SOVER)

LIBS   = $(LIB) $(SOLIBV)

UTILS   = $(USRCS:.c=)
SOUTILS = $(USRCS:.c=.shared)

NAMEPFX = udns-$(VERS)

CC = gcc
CFLAGS = -Wall -W -Wmissing-prototypes -O2 -fPIC
CDEFS = -DHAVE_POLL
PICFLAGS = -fPIC
AWK = awk

all: static

.SUFFIXES: .c .o .lo .shared

static: $(LIB) $(UTILS)
staticlib: $(LIB)
$(LIB): $(OBJS)
	-rm -f $@
	$(AR) rv $@ $(OBJS)
.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
$(OBJS): udns.h

$(UTILS): udns.h $(LIB)
.c:
	$(CC) $(CFLAGS) $(CDEFS) -o $@ $< $(LIB)

shared: $(SOLIBV) $(SOUTILS)
sharedlib: $(SOLIBV)

$(SOLIBV): $(SOBJS)
	$(CC) -shared -Wl,--soname,$(SOLIBV) -o $@ $(SOBJS)
	rm -f $(SOLIB)
	ln -s $(SOLIBV) $(SOLIB)
.c.lo:
	$(CC) $(CFLAGS) $(PICFLAGS) $(CDEFS) -o $@ -c $<
$(SOBJS): udns.h

$(SOUTILS): udns.h $(SOLIB)
.c.shared:
	$(CC) $(CFLAGS) $(CDEFS) -o $@ $< $(SOLIB)

# udns_codes.c is generated from udns.h
udns_codes.c:	udns.h Makefile
	@echo Generating $@
	@set -e; exec >$@.tmp; \
	set T type C class R rcode; \
	echo "/* Automatically generated. */"; \
	echo "#include \"udns.h\""; \
	while [ "$$1" ]; do \
	 echo; \
	 echo "const struct dns_nameval dns_$${2}tab[] = {"; \
	 $(AWK) "/^  DNS_$${1}_[A-Z0-9_]+[ 	]*=/ \
	  { printf \" {%s,\\\"%s\\\"},\\n\", \$$1, substr(\$$1,7) }" \
	  udns.h ; \
	 echo " {0,0}};"; \
	 echo "const char *dns_$${2}name(enum dns_$${2} code) {"; \
	 echo " static char nm[20];"; \
	 echo " switch(code) {"; \
	 $(AWK) "BEGIN{i=0} \
	   /^  DNS_$${1}_[A-Z0-9_]+[ 	]*=/ \
	   {printf \" case %s: return dns_$${2}tab[%d].name;\\n\",\$$1,i++}\
	   " udns.h ; \
	 echo " }"; \
	 echo " return _dns_format_code(nm,\"$$2\",code);"; \
	 echo "}"; \
	 shift 2; \
	done
	@mv $@.tmp $@

udns.3.html: udns.3
	groff -man -Thtml udns.3 > $@.tmp
	mv $@.tmp $@

dist: $(NAMEPFX).tar.gz
$(NAMEPFX).tar.gz: $(DIST) $(DEB)
	mkdir $(NAMEPFX) $(NAMEPFX)/debian
	ln $(DIST) $(NAMEPFX)
	ln $(DEB) $(NAMEPFX)/debian
	tar cvfz $@ $(NAMEPFX)
	rm -rf $(NAMEPFX)
subdist:
	cp -p $(DIST) $(TARGET)/

clean:
	rm -f $(OBJS) $(SOBJS) build-stamp
distclean: clean
	rm -f $(LIBS) libudns.so udns.3.html $(UTILS) $(SOUTILS)

.PHONY: all static staticlib shared sharedlib dist clean distclean subdist
