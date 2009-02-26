##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## as published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## License along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
## $Id: Makefile.am,v 1.5 2009/02/25 17:52:52 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

PBM2G3 = pbmtog3
FAX2TIFF = fax2tiff
TIFFCP = tiffcp

ITU_TEST_PAGES_PBM = itu1.pbm \
                     itu2.pbm \
                     itu3.pbm \
                     itu4.pbm \
                     itu5.pbm \
                     itu6.pbm \
                     itu7.pbm \
                     itu8.pbm \
                     test1.pbm \
                     test2.pbm \
                     test3.pbm \
                     test4.pbm

ITU_TEST_PAGES_G3 = itu1.g3 \
                    itu2.g3 \
                    itu3.g3 \
                    itu4.g3 \
                    itu5.g3 \
                    itu6.g3 \
                    itu7.g3 \
                    itu8.g3 \
                    test1.g3 \
                    test2.g3 \
                    test3.g3 \
                    test4.g3

ITU_TEST_PAGES = itu1.tif \
                 itu2.tif \
                 itu3.tif \
                 itu4.tif \
                 itu5.tif \
                 itu6.tif \
                 itu7.tif \
                 itu8.tif \
                 test1.tif \
                 test2.tif \
                 test3.tif \
                 test4.tif

MIXED_SIZE_PAGES = R1200_1200_A4.tif \
                   R1200_1200_B4.tif \
                   R1200_1200_A3.tif \
                   R600_1200_A4.tif \
                   R600_1200_B4.tif \
                   R600_1200_A3.tif \
                   R600_600_A4.tif \
                   R600_600_B4.tif \
                   R600_600_A3.tif \
                   R16_800_A4.tif \
                   R16_800_B4.tif \
                   R16_800_A3.tif \
                   R16_154_A4.tif \
                   R16_154_B4.tif \
                   R16_154_A3.tif \
                   R300_600_A4.tif \
                   R300_600_B4.tif \
                   R300_600_A3.tif \
                   R300_300_A4.tif \
                   R300_300_B4.tif \
                   R300_300_A3.tif \
                   R8_154_A4.tif \
                   R8_154_B4.tif \
                   R8_154_A3.tif \
                   R8_77_A4.tif \
                   R8_77_B4.tif \
                   R8_77_A3.tif \
                   R8_385_A4.tif \
                   R8_385_B4.tif \
                   R8_385_A3.tif

EXTRA_DIST = ${ITU_TEST_PAGES_PBM}

MAINTAINERCLEANFILES = Makefile.in

nobase_data_DATA =  itutests.tif \
                    ${ITU_TEST_PAGES} \
                    dithered.tif \
                    100pages.tif \
                    ${MIXED_SIZE_PAGES} \
                    mixed_size_pages.tif

noinst_PROGRAMS =   generate_dithered_tif \
                    generate_sized_pages

generate_dithered_tif_SOURCES = generate_dithered_tif.c
generate_dithered_tif_LDADD = -ltiff

generate_sized_pages_SOURCES = generate_sized_pages.c
generate_sized_pages_LDADD = -ltiff

clean:
	rm -f *.tif *.g3

itutests.tif: ${ITU_TEST_PAGES_G3}
	$(FAX2TIFF) -M -o $@ ${ITU_TEST_PAGES_G3}

100pages.tif: ${ITU_TEST_PAGES_G3}
	$(FAX2TIFF) -M -o $@ \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        ${ITU_TEST_PAGES_G3} \
        itu1.g3 itu2.g3 itu3.g3 itu4.g3

.g3.tif:
	${FAX2TIFF} -M -o $*.tif $*.g3

.pbm.g3:
	${PBM2G3} $*.pbm >$*.g3
    
dithered.tif: generate_dithered_tif$(EXEEXT)
	./generate_dithered_tif$(EXEEXT)

${MIXED_SIZE_PAGES}: generate_sized_pages$(EXEEXT)
	./generate_sized_pages$(EXEEXT)

mixed_size_pages.tif: ${MIXED_SIZE_PAGES}
	$(TIFFCP) ${MIXED_SIZE_PAGES} $@
