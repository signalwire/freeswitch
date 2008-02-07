##
## VoIPcodecs - a series of DSP components for telephony
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
## $Id: Makefile.am,v 1.85 2008/02/06 09:23:26 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_builddir)

lib_LTLIBRARIES = libvoipcodecs.la

libvoipcodecs_la_SOURCES = bitstream.c \
                           g711.c \
                           g722_encode.c \
                           g722_decode.c \
                           g726.c \
                           gsm0610_decode.c \
                           gsm0610_encode.c \
                           gsm0610_long_term.c \
                           gsm0610_lpc.c \
                           gsm0610_preprocess.c \
                           gsm0610_rpe.c \
                           gsm0610_short_term.c \
                           ima_adpcm.c \
                           lpc10_analyse.c \
                           lpc10_decode.c \
                           lpc10_encode.c \
                           lpc10_placev.c \
                           lpc10_voicing.c \
                           oki_adpcm.c \
                           vector_int.c

libvoipcodecs_la_LDFLAGS = -version-info @VOIPCODECS_LT_CURRENT@:@VOIPCODECS_LT_REVISION@:@VOIPCODECS_LT_AGE@

nobase_include_HEADERS = voipcodecs/bit_operations.h \
                         voipcodecs/bitstream.h \
                         voipcodecs/dc_restore.h \
                         voipcodecs/g711.h \
                         voipcodecs/g722.h \
                         voipcodecs/g726.h \
                         voipcodecs/gsm0610.h \
                         voipcodecs/ima_adpcm.h \
                         voipcodecs/lpc10.h \
                         voipcodecs/oki_adpcm.h \
                         voipcodecs/telephony.h \
                         voipcodecs/vector_int.h \
                         voipcodecs/version.h

nodist_include_HEADERS = voipcodecs.h

noinst_HEADERS = gsm0610_local.h \
                 lpc10_encdecs.h

# We need to run at_dictionary_gen, so it generates the
# at_interpreter_dictionary.h file

DSP = libvoipcodecs.dsp
VCPROJ = libvoipcodecs.vcproj

WIN32SOURCES = $(libvoipcodecs_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) voipcodecs.h

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

voipcodecs/version.h:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$VOIPCODECS_RELEASE_DATE/'$$NOWDATE'/;s/$$VOIPCODECS_RELEASE_TIME/'$$NOWTIME'/' \
		<voipcodecs/version.h.in >voipcodecs/version.h

dist-hook:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$VOIPCODECS_RELEASE_DATE/'$$NOWDATE'/;s/$$VOIPCODECS_RELEASE_TIME/'$$NOWTIME'/' \
		<voipcodecs/version.h.in >voipcodecs/version.h
