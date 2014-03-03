##
## g722_1 - a library for the ITU G.722.1 and Annex C codecs
##
## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

LIBS += $(TESTLIBS)

EXTRA_DIST = regression_tests.sh

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_builddir)/src -DDATADIR="\"$(pkgdatadir)\""

LIBDIR = -L$(top_builddir)/src

noinst_PROGRAMS = g722_1_tests

noinst_HEADERS =  g192_bit_stream.h \
                  timing.h

g722_1_tests_SOURCES = g722_1_tests.c g192_bit_stream.c
g722_1_tests_LDADD = $(LIBDIR) -lg722_1
