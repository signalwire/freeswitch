##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
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
## $Id: Makefile.am,v 1.7 2008/05/03 13:05:04 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

LIBS += $(TESTLIBS)

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_builddir) -I$(top_builddir)/src -DDATADIR="\"$(pkgdatadir)\""

noinst_PROGRAMS = make_line_models

lib_LTLIBRARIES = libspandsp-sim.la

libspandsp_sim_la_SOURCES = g1050.c \
                            line_model.c \
                            rfc2198_sim.c \
                            test_utils.c

nodist_libspandsp_sim_la_SOURCES = line_models.c

libspandsp_sim_la_LDFLAGS = -version-info @SPANDSP_LT_CURRENT@:@SPANDSP_LT_REVISION@:@SPANDSP_LT_AGE@

nobase_include_HEADERS = spandsp/g1050.h \
                         spandsp/line_model.h \
                         spandsp/line_models.h \
                         spandsp/rfc2198_sim.h \
                         spandsp/test_utils.h \
                         spandsp-sim.h

make_line_models_SOURCES = make_line_models.c
make_line_models_LDADD = -L$(top_builddir)/src -lspandsp

# We need to run make_line_models, so it generates the line_models.h file
# used by several of the test programs.

line_models.lo: make_line_models$(EXEEXT) line_models.c

line_models.$(OBJEXT): make_line_models$(EXEEXT) line_models.c

line_models.c: make_line_models$(EXEEXT)
	./make_line_models$(EXEEXT)
    
DSP = libspandsp-sim.dsp
VCPROJ = libspandsp-sim.vcproj

WIN32SOURCES = $(libspandsp_sim_la_SOURCES) msvc/gettimeofday.c
WIN32HEADERS = $(nobase_include_HEADERS) spandsp-sim.h

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
