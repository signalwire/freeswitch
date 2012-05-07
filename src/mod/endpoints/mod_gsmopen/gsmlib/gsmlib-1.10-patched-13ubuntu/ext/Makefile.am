## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: Makefile for phone-specific extensions
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 16.12.2001
# *************************************************************************

INCLUDES =		-I..

EXTRA_DIST =		README.sieme

# build addon library
lib_LTLIBRARIES =	libgsmext.la

libgsmext_la_SOURCES =	gsm_sie_me.cc

libgsmext_la_LDFLAGS =	-version-info $(GSM_VERSION)

gsmincludedir =		$(includedir)/gsmlib

gsminclude_HEADERS =	gsm_sie_me.h

# build programs
bin_PROGRAMS =		gsmsiectl gsmsiexfer

# build gsmsiectl from gsmsiectl.cc and libgsmme.la
gsmsiectl_SOURCES =	gsmsiectl.cc
gsmsiectl_LDADD =	../gsmlib/libgsmme.la libgsmext.la $(INTLLIBS)

# build gsmsiexfer from gsmsiexfer.cc and libgsmme.la
gsmsiexfer_SOURCES =	gsmsiexfer.cc
gsmsiexfer_LDADD =	../gsmlib/libgsmme.la libgsmext.la $(INTLLIBS)


