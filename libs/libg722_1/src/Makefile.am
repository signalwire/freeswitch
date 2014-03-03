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

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = make_tables.c \
             g722_1/version.h.in \
             libg722_1.dsp \
             libg722_1.2005.sln \
             libg722_1.2008.sln \
             libg722_1.2005.vcproj \
             libg722_1.2008.vcproj \
             msvc/gettimeofday.c \
             msvc/inttypes.h \
             msvc/tgmath.h \
             msvc/unistd.h \
             msvc/sys/time.h \
             msvc/g722_1.def \
             msvc/msvcproj.head \
             msvc/msvcproj.foot \
             msvc/vc8proj.head \
             msvc/vc8proj.foot \
             msvc/vc9proj.head \
             msvc/vc9proj.foot

AM_CPPFLAGS = -I$(top_builddir)

lib_LTLIBRARIES = libg722_1.la

libg722_1_la_SOURCES = basop32.c \
                       bitstream.c \
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
                       tables.c \
                       utilities.c

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
                 tables.h \
                 utilities.h

noinst_PROGRAMS = make_dct4_tables

dct4.$(OBJEXT): dct4.h

dct4.lo: dct4.h

dct4.h: make_dct4_tables$(EXEEXT)
	./make_dct4_tables$(EXEEXT) >dct4.h

make_dct4_tables$(EXEEXT): $(top_srcdir)/src/make_dct4_tables.c
	$(CC_FOR_BUILD) -o make_dct4_tables$(EXEEXT) $(top_srcdir)/src/make_dct4_tables.c  -DHAVE_CONFIG_H -I$(top_builddir)/src -lm

#coef2sam.h: make_tables$(EXEEXT)
#	./make_tables$(EXEEXT) coef2sam >coef2samx.h

#sam2coef.h: make_tables$(EXEEXT)
#	./make_tables$(EXEEXT) sam2coef >sam2coefx.h

DSP = libg722_1.dsp
VCPROJ8 = libg722_1.2005.vcproj
VCPROJ9 = libg722_1.2008.vcproj

WIN32SOURCES = $(libg722_1_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) g722_1.h

DSPOUT = | awk '{printf("%s\r\n", $$0)}' >> $(DSP)
VCPROJOUT8 = | awk '{printf("%s\r\n", $$0)}' >> $(VCPROJ8)
VCPROJOUT9 = | awk '{printf("%s\r\n", $$0)}' >> $(VCPROJ9)

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

$(VCPROJ8): msvc/vc8proj.head msvc/vc8proj.foot Makefile.am
	echo "creating $(VCPROJ8)"
	@(cp $(srcdir)/msvc/vc8proj.head $(VCPROJ8); \
	for file in $(WIN32SOURCES); do \
    myfile=`echo $$file | sed -e 's|/|\\\\|g'`; \
    echo "<File RelativePath=\""$$myfile"\"></File>" $(VCPROJOUT8); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT8); \
	for file in $(WIN32HEADERS); do \
        myfile=`echo $$file | sed -e 's|/|\\\\|g'`; \
		echo "<File RelativePath=\""$$myfile"\"></File>" $(VCPROJOUT8); \
	done; \
	cat $(srcdir)/msvc/vc8proj.foot $(VCPROJOUT8) )

$(VCPROJ9): msvc/vc9proj.head msvc/vc9proj.foot Makefile.am
	echo "creating $(VCPROJ9)"
	@(cp $(srcdir)/msvc/vc9proj.head $(VCPROJ9); \
	for file in $(WIN32SOURCES); do \
        myfile=`echo $$file | sed -e 's|/|\\\\|g'`; \
		echo "<File RelativePath=\""$$myfile"\"></File>" $(VCPROJOUT9); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT9); \
	for file in $(WIN32HEADERS); do \
        myfile=`echo $$file | sed -e 's|/|\\\\|g'`; \
		echo "<File RelativePath=\""$$myfile"\"></File>" $(VCPROJOUT9); \
	done; \
	cat $(srcdir)/msvc/vc9proj.foot $(VCPROJOUT9) )

dist-hook: g722_1/version.h

g722_1/version.h:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$G722_1_RELEASE_DATE/'$$NOWDATE'/;s/$$G722_1_RELEASE_TIME/'$$NOWTIME'/' \
		<$(srcdir)/g722_1/version.h.in >$@
