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
## $Id: Makefile.am,v 1.10 2009/06/02 16:03:55 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

LIBS += -L$(top_builddir)/src -lspandsp $(SIMLIBS)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = libspandsp_sim.dsp \
             libspandsp_sim.2005.vcproj \
             libspandsp_sim.2008.vcproj \
             msvc/make_line_models.2008.vcproj \
             msvc/msvcproj.head \
             msvc/msvcproj.foot \
             msvc/vc8proj.head \
             msvc/vc8proj.foot \
             msvc/vc9proj.head \
             msvc/vc9proj.foot

INCLUDES = -I$(top_builddir) -I$(top_builddir)/src -DDATADIR="\"$(pkgdatadir)\""

noinst_PROGRAMS = make_line_models

lib_LTLIBRARIES = libspandsp-sim.la

libspandsp_sim_la_SOURCES = g1050.c \
                            line_model.c \
                            rfc2198_sim.c \
                            test_utils.c

nodist_libspandsp_sim_la_SOURCES = line_models.c

libspandsp_sim_la_LDFLAGS = -version-info @SPANDSP_LT_CURRENT@:@SPANDSP_LT_REVISION@:@SPANDSP_LT_AGE@ $(COMP_VENDOR_LDFLAGS)

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
    
DSP = libspandsp_sim.dsp
VCPROJ8 = libspandsp_sim.2005.vcproj
VCPROJ9 = libspandsp_sim.2008.vcproj

WIN32SOURCES = $(libspandsp_sim_la_SOURCES)
WIN32HEADERS = $(nobase_include_HEADERS)

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
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT8); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT8); \
	for file in $(WIN32HEADERS); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT8); \
	done; \
	cat $(srcdir)/msvc/vc8proj.foot $(VCPROJOUT8) )

$(VCPROJ9): msvc/vc9proj.head msvc/vc9proj.foot Makefile.am
	echo "creating $(VCPROJ9)"
	@(cp $(srcdir)/msvc/vc9proj.head $(VCPROJ9); \
	for file in $(WIN32SOURCES); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT9); \
	done; \
	echo "</Filter><Filter  Name=\"Header Files\">" $(VCPROJOUT9); \
	for file in $(WIN32HEADERS); do \
		echo "<File RelativePath=\""$$file"\"></File>" $(VCPROJOUT9); \
	done; \
	cat $(srcdir)/msvc/vc9proj.foot $(VCPROJOUT9) )
