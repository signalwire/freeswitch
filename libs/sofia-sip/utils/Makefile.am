#
# Makefile.am for sofia-sip/utils
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#
# ref: http://www.gnu.org/software/automake/manual/automake.html

# ----------------------------------------------------------------------
# Header paths

include $(top_srcdir)/rules/sofia.am

INCLUDES = 	${INTERNAL_INCLUDES}

# ----------------------------------------------------------------------
# Build targets

noinst_HEADERS = 	apps_utils.h

bin_PROGRAMS =		sip-options sip-date sip-dig

LDADD =			../libsofia-sip-ua/libsofia-sip-ua.la $(GLIB_LIBS)

# ----------------------------------------------------------------------
# Dist and install
EXTRA_DIST = utils.docs

CLEANFILES = utils.doxytags