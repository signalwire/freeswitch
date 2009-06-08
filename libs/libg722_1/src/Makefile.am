##
## g722_1 - a library for the ITU G.722.1 and Annex C codecs
##
## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the Lesser GNU General Public License version 2.1, as
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
## $Id: Makefile.am,v 1.10 2008/10/16 15:46:12 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = g722_1/version.h.in \
             libg722_1.dsp \
             libg722_1.sln \
             libg722_1.vcproj \
             msvc/gettimeofday.c \
             msvc/inttypes.h \
             msvc/tgmath.h \
             msvc/unistd.h \
             msvc/sys/time.h \
             msvc/g722_1.def \
             msvc/msvcproj.head \
             msvc/msvcproj.foot \
             msvc/vc8proj.head \
             msvc/vc8proj.foot

INCLUDES = -I$(top_builddir)

lib_LTLIBRARIES = libg722_1.la

libg722_1_la_SOURCES = bitstream.c \
                       coef2sam.c \
                       common.c \
                       commonf.c \
                       dct4.c \
                       dct4_a.c \
                       dct4_s.c \
                       decoder.c \
                       decoderf.c \
                       encoder.c \
                       encoderf.c \
                       huff_tab.c \
                       sam2coef.c \
                       tables.c

if COND_FIXED
libg722_1_la_SOURCES += basop32.c
endif
libg722_1_la_LDFLAGS = -version-info @G722_1_LT_CURRENT@:@G722_1_LT_REVISION@:@G722_1_LT_AGE@ $(COMP_VENDOR_LDFLAGS)

nobase_include_HEADERS = g722_1/g722_1.h \
                         g722_1/version.h

nodist_include_HEADERS = g722_1.h

noinst_HEADERS = basop32.h \
                 bitstream.h \
                 coef2sam.h \
                 dct4.h \
                 dct4_a.h \
                 dct4_s.h \
                 defs.h \
                 huff_tab.h \
                 sam2coef.h \
                 tables.h

noinst_PROGRAMS = make_dct4_tables \
                  make_tables

dct4.$(OBJEXT): dct4.h

dct4.lo: dct4.h

dct4.h: make_dct4_tables$(EXEEXT)
	./make_dct4_tables$(EXEEXT) >dct4.h

#coef2sam.h: make_tables$(EXEEXT)
#	./make_tables$(EXEEXT) coef2sam >coef2samx.h

#sam2coef.h: make_tables$(EXEEXT)
#	./make_tables$(EXEEXT) sam2coef >sam2coefx.h

DSP = libg722_1.dsp
VCPROJ = libg722_1.vcproj

WIN32SOURCES = $(libg722_1_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) g722_1.h

DSPOUT = | awk '{printf("%s\r\n", $$0)}' >> $(DSP)
VCPROJOUT = | awk '{printf("%s\r\n", $$0)}' >> $(VCPROJ)

$(DSP): msvc/msvcproj.head msvc/msvcproj.foot Makefile.am
	echo "creating $(DSP)"
	@(cp $(srcdir)/msvc/msvcproj.head $(DSP); \
	echo "# Begin Group \"Source Files\"" $(DSPOUT); \
	for file in $(WIN32SOURCES); do \
	echo "# Begin Source File" $(DSPOUT); \
	echo "" $(DSPOUT); \
	echo "SOURCE=.\\"$$file $(DSPOUT); \
	echo "# End Source File" $(DSPOUT); \
	done; \
	echo "# End Group" $(DSPOUT); \
	echo "# Begin Group \"Header Files\"" $(DSPOUT); \
	for file in $(WIN32HEADERS); do \
	echo "# Begin Source File" $(DSPOUT); \
	echo "" $(DSPOUT); \
	echo "SOURCE=.\\"$$file $(DSPOUT); \
	echo "# End Source File" $(DSPOUT); \
	done; \
	echo "# End Group" $(DSPOUT); \
	cat $(srcdir)/msvc/msvcproj.foot $(DSPOUT) )

$(VCPROJ): msvc/vc8proj.head msvc/vc8proj.foot Makefile.am
	echo "creating $(VCPROJ)"
	@(cp $(srcdir)/msvc/vc8proj.head $(VCPROJ); \
	for file in $(WIN32SOURCES); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT); \
	for file in $(WIN32HEADERS); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT); \
	done; \
	cat $(srcdir)/msvc/vc8proj.foot $(VCPROJOUT) )

g722_1/version.h:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$G722_1_RELEASE_DATE/'$$NOWDATE'/;s/$$G722_1_RELEASE_TIME/'$$NOWTIME'/' \
		<g722_1/version.h.in >g722_1/version.h

dist-hook:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$G722_1_RELEASE_DATE/'$$NOWDATE'/;s/$$G722_1_RELEASE_TIME/'$$NOWTIME'/' \
		<g722_1/version.h.in >g722_1/version.h
