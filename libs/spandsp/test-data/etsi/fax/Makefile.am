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

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)

PBM2G3 = pbmtog3
FAX2TIFF = fax2tiff
TIFFCP = tiffcp

ETSI_TEST_PAGES = etsi_300_242_a4_diago1.tif \
                  etsi_300_242_a4_diago2.tif \
                  etsi_300_242_a4_duration1.tif \
                  etsi_300_242_a4_duration2.tif \
                  etsi_300_242_a4_error.tif \
                  etsi_300_242_a4_impress.tif \
                  etsi_300_242_a4_impress_white.tif \
                  etsi_300_242_a4_stairstep.tif \
                  etsi_300_242_a4_white.tif \
                  etsi_300_242_a4_white_2p.tif

EXTRA_DIST = 

MAINTAINERCLEANFILES = Makefile.in

LIBDIR = -L$(top_builddir)/src

nobase_data_DATA = ${ETSI_TEST_PAGES}
                   
noinst_PROGRAMS = generate_etsi_300_242_pages

generate_etsi_300_242_pages_SOURCES = generate_etsi_300_242_pages.c
generate_etsi_300_242_pages_LDADD = $(LIBDIR) -lspandsp -ltiff

clean:
	rm -f *.tif *.g3

.g3.tif:
	${FAX2TIFF} -M -o $*.tif $*.g3

.pbm.g3:
	${PBM2G3} $*.pbm >$*.g3

${ETSI_TEST_PAGES}: generate_etsi_300_242_pages$(EXEEXT)
	./generate_etsi_300_242_pages$(EXEEXT)
