##
## iLBC - a library for the iLBC codec
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
## $Id: Makefile.am,v 1.1.1.1 2008/02/15 12:15:55 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_builddir)

lib_LTLIBRARIES = libilbc.la

libilbc_la_SOURCES = anaFilter.c \
                     constants.c \
                     createCB.c \
                     doCPLC.c \
                     enhancer.c \
                     filter.c \
                     FrameClassify.c \
                     gainquant.c \
                     getCBvec.c \
                     helpfun.c \
                     hpInput.c \
                     hpOutput.c \
                     iCBConstruct.c \
                     iCBSearch.c \
                     iLBC_decode.c \
                     iLBC_encode.c \
                     LPCdecode.c \
                     LPCencode.c \
                     lsf.c \
                     packing.c \
                     StateConstructW.c \
                     StateSearchW.c \
                     syntFilter.c

libilbc_la_LDFLAGS = -version-info @ILBC_LT_CURRENT@:@ILBC_LT_REVISION@:@ILBC_LT_AGE@

nobase_include_HEADERS = ilbc/ilbc.h \
                         ilbc/version.h

nodist_include_HEADERS = ilbc.h

noinst_HEADERS = anaFilter.h \
                 constants.h \
                 createCB.h \
                 doCPLC.h \
                 enhancer.h \
                 filter.h \
                 FrameClassify.h \
                 gainquant.h \
                 getCBvec.h \
                 helpfun.h \
                 hpInput.h \
                 hpOutput.h \
                 iCBConstruct.h \
                 iCBSearch.h \
                 iLBC_define.h \
                 LPCdecode.h \
                 LPCencode.h \
                 lsf.h \
                 packing.h \
                 StateConstructW.h \
                 StateSearchW.h \
                 syntFilter.h

# We need to run at_dictionary_gen, so it generates the
# at_interpreter_dictionary.h file

DSP = libilbc.dsp
VCPROJ = libilbc.vcproj

WIN32SOURCES = $(libilbc_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) ilbc.h

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

ilbc/version.h:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$ilbc_RELEASE_DATE/'$$NOWDATE'/;s/$$ilbc_RELEASE_TIME/'$$NOWTIME'/' \
		<ilbc/version.h.in >ilbc/version.h

dist-hook:
	NOWDATE=`date --utc +"%Y%m%d"` ; \
	NOWTIME=`date --utc +"%H%M%S"` ; \
	sed 's/$$ilbc_RELEASE_DATE/'$$NOWDATE'/;s/$$ilbc_RELEASE_TIME/'$$NOWTIME'/' \
		<ilbc/version.h.in >ilbc/version.h
