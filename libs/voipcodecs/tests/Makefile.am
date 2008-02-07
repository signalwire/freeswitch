##
## SpanDSP - a series of DSP components for telephony
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
##
## $Id: Makefile.am,v 1.94 2008/01/31 13:34:40 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)

LIBS += $(TESTLIBS)

EXTRA_DIST = regression_tests.sh
 
MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_builddir)/src -DDATADIR="\"$(pkgdatadir)\""

LIBDIR = -L$(top_builddir)/src

noinst_PROGRAMS =   g711_tests \
                    g722_tests \
                    g726_tests \
                    gsm0610_tests \
                    ima_adpcm_tests \
                    lpc10_tests \
                    oki_adpcm_tests

g711_tests_SOURCES = g711_tests.c
g711_tests_LDADD = $(LIBDIR) -lvoipcodecs -lspandsp

g722_tests_SOURCES = g722_tests.c
g722_tests_LDADD = $(LIBDIR) -lvoipcodecs

g726_tests_SOURCES = g726_tests.c
g726_tests_LDADD = $(LIBDIR) -lvoipcodecs

gsm0610_tests_SOURCES = gsm0610_tests.c
gsm0610_tests_LDADD = $(LIBDIR) -lvoipcodecs

ima_adpcm_tests_SOURCES = ima_adpcm_tests.c
ima_adpcm_tests_LDADD = $(LIBDIR) -lvoipcodecs

lpc10_tests_SOURCES = lpc10_tests.c
lpc10_tests_LDADD = $(LIBDIR) -lvoipcodecs

oki_adpcm_tests_SOURCES = oki_adpcm_tests.c
oki_adpcm_tests_LDADD = $(LIBDIR) -lvoipcodecs
