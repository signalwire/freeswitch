##
## broadvoice - a library for the BroadVoice 16 and 32 codecs
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
## $Id: Makefile.am,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = broadvoice/version.h.in \
             broadvoice.dsp \
             broadvoice.sln \
             broadvoice.vcproj \
             msvc/gettimeofday.c \
             msvc/inttypes.h \
             msvc/tgmath.h \
             msvc/unistd.h \
             msvc/sys/time.h \
             msvc/broadvoice.def \
             msvc/msvcproj.head \
             msvc/msvcproj.foot \
             msvc/vc8proj.head \
             msvc/vc8proj.foot

INCLUDES = -I$(top_builddir) -Ifloating/common -Ifloating/bv16 -Ifloating/bv32

lib_LTLIBRARIES = libbroadvoice.la

libbroadvoice_la_SOURCES = bitstream.c \
                           floating/common/a2lsp.c \
                           floating/common/allpole.c \
                           floating/common/allzero.c \
                           floating/common/autocor.c \
                           floating/common/cmtables.c \
                           floating/common/levdur.c \
                           floating/common/lsp2a.c \
                           floating/common/stblchck.c \
                           floating/common/stblzlsp.c \
                           floating/common/utility.c \
                           bitpack16.c \
                           floating/bv16/bv16decoder.c \
                           floating/bv16/bv16encoder.c \
                           floating/bv16/bv16coarse_pitch.c \
                           floating/bv16/bv16excdec.c \
                           floating/bv16/bv16excquan.c \
                           floating/bv16/bv16fine_pitch.c \
                           floating/bv16/bv16gaindec.c \
                           floating/bv16/bv16gainquan.c \
                           floating/bv16/bv16levelest.c \
                           floating/bv16/bv16lspdec.c \
                           floating/bv16/bv16lspquan.c \
                           floating/bv16/bv16plc.c \
                           floating/bv16/bv16ptdec.c \
                           floating/bv16/bv16postfilter.c \
                           floating/bv16/bv16ptquan.c \
                           floating/bv16/bv16tables.c \
                           bitpack32.c \
                           floating/bv32/bv32decoder.c \
                           floating/bv32/bv32encoder.c \
                           floating/bv32/bv32coarse_pitch.c \
                           floating/bv32/bv32excdec.c \
                           floating/bv32/bv32excquan.c \
                           floating/bv32/bv32fine_pitch.c \
                           floating/bv32/bv32gaindec.c \
                           floating/bv32/bv32gainquan.c \
                           floating/bv32/bv32levelest.c \
                           floating/bv32/bv32lspdec.c \
                           floating/bv32/bv32lspquan.c \
                           floating/bv32/bv32plc.c \
                           floating/bv32/bv32ptdec.c \
                           floating/bv32/bv32ptquan.c \
                           floating/bv32/bv32tables.c

libbroadvoice_la_LDFLAGS = -version-info @BROADVOICE_LT_CURRENT@:@BROADVOICE_LT_REVISION@:@BROADVOICE_LT_AGE@ $(COMP_VENDOR_LDFLAGS)

nobase_include_HEADERS = broadvoice/broadvoice.h \
                         broadvoice/version.h \
                         broadvoice/private/broadvoice.h

nodist_include_HEADERS = broadvoice.h

noinst_HEADERS = bitpack16.h \
                 bitpack32.h \
                 bitstream.h \
                 floating/bv16/bv16cnst.h \
                 floating/bv16/bv16externs.h \
                 floating/bv16/bv16postfilter.h \
                 floating/bv16/bv16strct.h \
                 floating/bv32/bv32cnst.h \
                 floating/bv32/bv32externs.h \
                 floating/bv32/bv32strct.h \
                 floating/common/bvcommon.h \
                 floating/common/typedef.h \
                 floating/common/utility.h

noinst_PROGRAMS =

DSP = broadvoice.dsp
VCPROJ = broadvoice.vcproj

WIN32SOURCES = $(libbroadvoice_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) broadvoice.h

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

broadvoice/version.h:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$BROADVOICE_RELEASE_DATE/'$$NOWDATE'/;s/$$BROADVOICE_RELEASE_TIME/'$$NOWTIME'/' \
		<broadvoice/version.h.in >broadvoice/version.h

dist-hook:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$BROADVOICE_RELEASE_DATE/'$$NOWDATE'/;s/$$BROADVOICE_RELEASE_TIME/'$$NOWTIME'/' \
		<broadvoice/version.h.in >broadvoice/version.h
