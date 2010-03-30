## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: Toplevel Makefile
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 21.5.1999
# *************************************************************************

SUBDIRS_ =	po gsmlib apps tests doc scripts win32 ext

EXTRA_DIST =	gsmlib.spec

if COMPILE_INTL
SUBDIRS =	intl $(SUBDIRS_) # po - make automake happy
else
SUBDIRS =	$(SUBDIRS_) # po intl - make automake happy
endif

all:
