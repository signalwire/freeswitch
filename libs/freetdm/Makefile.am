# Copyright (c) 2007, Anthony Minessale II
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# * Neither the name of the original author; nor the names of any contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

PREFIX = $(prefix)
SRC    = src

moddir = @modinstdir@
libdir = @libdir@
library_includedir = $(PREFIX)/include

INCS      = -I$(FT_SRCDIR)/$(SRC)/include -I$(FT_SRCDIR)/$(SRC)/isdn/include
if HAVE_SCTP
INCS +=  -I$(FT_SRCDIR)/$(SRC)/ftmod/ftmod_sangoma_boost 
endif
MY_CFLAGS = $(INCS) $(FTDM_CFLAGS) -DFTDM_CONFIG_DIR=\"@confdir@\" -DFTDM_MOD_DIR=\"$(moddir)\" @COMP_VENDOR_CFLAGS@ @DEFS@
COMPILE   = $(CC) $(MY_CFLAGS) $(INCS)
LTCOMPILE = $(LIBTOOL) --mode=compile --tag=CC $(COMPILE)
LINK      = $(LIBTOOL) --mode=link --tag=CC $(CC) $(MY_CFLAGS) $(LDFLAGS) -o $@


#
# GNU pkgconfig file
#
EXTRA_DIST = freetdm.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = freetdm.pc


#
# libfreetdm
#
libfreetdm_la_SOURCES = \
$(SRC)/hashtable.c \
$(SRC)/hashtable_itr.c \
$(SRC)/ftdm_io.c \
$(SRC)/ftdm_queue.c \
$(SRC)/ftdm_config.c \
$(SRC)/ftdm_callerid.c \
$(SRC)/fsk.c \
$(SRC)/uart.c \
$(SRC)/g711.c \
$(SRC)/libteletone_detect.c \
$(SRC)/libteletone_generate.c \
$(SRC)/ftdm_buffer.c \
$(SRC)/ftdm_threadmutex.c \
$(SRC)/ftdm_dso.c \
$(SRC)/ftdm_cpu_monitor.c

library_include_HEADERS = \
$(SRC)/include/fsk.h \
$(SRC)/include/g711.h \
$(SRC)/include/hashtable.h \
$(SRC)/include/hashtable_itr.h \
$(SRC)/include/hashtable_private.h \
$(SRC)/include/libteletone_detect.h \
$(SRC)/include/libteletone_generate.h \
$(SRC)/include/libteletone.h \
$(SRC)/include/freetdm.h \
$(SRC)/include/sangoma_tdm_api.h \
$(SRC)/include/uart.h \
$(SRC)/include/ftdm_buffer.h \
$(SRC)/include/ftdm_config.h \
$(SRC)/include/ftdm_threadmutex.h \
$(SRC)/include/ftdm_dso.h \
$(SRC)/include/ftdm_types.h \
$(SRC)/include/ftdm_cpu_monitor.h 

lib_LTLIBRARIES	       = libfreetdm.la
libfreetdm_la_CFLAGS   = $(AM_CFLAGS) $(MY_CFLAGS)
libfreetdm_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libfreetdm_la_LIBADD   = $(LIBS)

MYLIB = libfreetdm.la

core: libfreetdm.la 
core-install: install-libLTLIBRARIES

#
# tools & test programs
#
noinst_PROGRAMS = testtones detect_tones detect_dtmf testpri testr2 testanalog testapp testcid #testisdn
if HAVE_SCTP
noinst_PROGRAMS += testboost
endif
noinst_PROGRAMS += testsangomaboost

testapp_SOURCES = $(SRC)/testapp.c
testapp_LDADD   = libfreetdm.la
testapp_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testcid_SOURCES = $(SRC)/testcid.c
testcid_LDADD   = libfreetdm.la
testcid_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testtones_SOURCES = $(SRC)/testtones.c
testtones_LDADD   = libfreetdm.la
testtones_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

detect_tones_SOURCES = $(SRC)/detect_tones.c
detect_tones_LDADD   = libfreetdm.la
detect_tones_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

detect_dtmf_SOURCES = $(SRC)/detect_dtmf.c
detect_dtmf_LDADD   = libfreetdm.la
detect_dtmf_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

#testisdn_SOURCES = $(SRC)/testisdn.c
#testisdn_LDADD   = libfreetdm.la
#testisdn_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testpri_SOURCES = $(SRC)/testpri.c
testpri_LDADD   = libfreetdm.la
testpri_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testr2_SOURCES = $(SRC)/testr2.c
testr2_LDADD   = libfreetdm.la
testr2_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

if HAVE_SCTP
testboost_SOURCES = $(SRC)/testboost.c
testboost_LDADD   = libfreetdm.la
testboost_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
endif

testsangomaboost_SOURCES = $(SRC)/testsangomaboost.c
testsangomaboost_LDADD   = libfreetdm.la
testsangomaboost_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testanalog_SOURCES = $(SRC)/testanalog.c
testanalog_LDADD   = libfreetdm.la
testanalog_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

#
# ftmod modules
#
mod_LTLIBRARIES = ftmod_zt.la ftmod_skel.la ftmod_analog.la ftmod_analog_em.la #ftmod_isdn.la


if HAVE_SCTP 
mod_LTLIBRARIES += ftmod_sangoma_boost.la
endif

if LIBSANGOMA
mod_LTLIBRARIES += ftmod_wanpipe.la
endif

if LIBPRI
mod_LTLIBRARIES += ftmod_libpri.la
endif

if OPENR2
mod_LTLIBRARIES += ftmod_r2.la
endif

ftmod_zt_la_SOURCES = $(SRC)/ftmod/ftmod_zt/ftmod_zt.c
ftmod_zt_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_zt_la_LDFLAGS = -module -avoid-version
ftmod_zt_la_LIBADD  = $(MYLIB)

ftmod_skel_la_SOURCES = $(SRC)/ftmod/ftmod_skel/ftmod_skel.c
ftmod_skel_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_skel_la_LDFLAGS = -module -avoid-version
ftmod_skel_la_LIBADD  = $(MYLIB)

if LIBSANGOMA
ftmod_wanpipe_la_SOURCES = $(SRC)/ftmod/ftmod_wanpipe/ftmod_wanpipe.c
ftmod_wanpipe_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS) -D__LINUX__ -I/usr/include/wanpipe
ftmod_wanpipe_la_LDFLAGS = -module -avoid-version -lsangoma
ftmod_wanpipe_la_LIBADD  = $(MYLIB)
endif

#ftmod_isdn_la_SOURCES = \
#$(SRC)/isdn/EuroISDNStateNT.c \
#$(SRC)/isdn/EuroISDNStateTE.c \
#$(SRC)/isdn/mfifo.c \
#$(SRC)/isdn/Q921.c \
#$(SRC)/isdn/Q931api.c \
#$(SRC)/isdn/Q931.c \
#$(SRC)/isdn/Q931ie.c \
#$(SRC)/isdn/Q931mes.c \
#$(SRC)/isdn/Q931StateNT.c \
#$(SRC)/isdn/Q931StateTE.c \
#$(SRC)/isdn/nationalmes.c \
#$(SRC)/isdn/nationalStateNT.c \
#$(SRC)/isdn/nationalStateTE.c \
#$(SRC)/isdn/DMSmes.c \
#$(SRC)/isdn/DMSStateNT.c \
#$(SRC)/isdn/DMSStateTE.c \
#$(SRC)/isdn/5ESSmes.c \
#$(SRC)/isdn/5ESSStateNT.c \
#$(SRC)/isdn/5ESSStateTE.c \
#$(SRC)/isdn/Q932mes.c \
#$(SRC)/ftmod/ftmod_isdn/ftmod_isdn.c

#ftmod_isdn_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS) -D_GNU_SOURCE
#ftmod_isdn_la_LDFLAGS = $(PCAP_LIB_FLAGS) -module -avoid-version
#ftmod_isdn_la_LIBADD  = $(MYLIB)

ftmod_analog_la_SOURCES = $(SRC)/ftmod/ftmod_analog/ftmod_analog.c
ftmod_analog_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_analog_la_LDFLAGS = -module -avoid-version
ftmod_analog_la_LIBADD  = $(MYLIB)

ftmod_analog_em_la_SOURCES = $(SRC)/ftmod/ftmod_analog_em/ftmod_analog_em.c
ftmod_analog_em_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_analog_em_la_LDFLAGS = -module -avoid-version
ftmod_analog_em_la_LIBADD  = $(MYLIB)

if HAVE_SCTP
ftmod_sangoma_boost_la_SOURCES = $(SRC)/ftmod/ftmod_sangoma_boost/sangoma_boost_client.c $(SRC)/ftmod/ftmod_sangoma_boost/ftmod_sangoma_boost.c
ftmod_sangoma_boost_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_sangoma_boost_la_LDFLAGS = -module -avoid-version
ftmod_sangoma_boost_la_LIBADD  = $(MYLIB)
endif

if LIBPRI
ftmod_libpri_la_SOURCES = $(SRC)/ftmod/ftmod_libpri/ftmod_libpri.c $(SRC)/ftmod/ftmod_libpri/lpwrap_pri.c
ftmod_libpri_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_libpri_la_LDFLAGS = -module  -avoid-version -lpri
ftmod_libpri_la_LIBADD  = $(MYLIB)
endif

if OPENR2
ftmod_r2_la_SOURCES = $(SRC)/ftmod/ftmod_r2/ftmod_r2.c 
ftmod_r2_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ftmod_r2_la_LDFLAGS = -module  -avoid-version -lopenr2
ftmod_r2_la_LIBADD  = $(MYLIB)
endif


dox doxygen:
	cd docs && doxygen $(FT_SRCDIR)/docs/Doxygen.conf

mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN): $(MYLIB) mod_freetdm/mod_freetdm.c
	cd mod_freetdm && make

mod_freetdm: mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN)

mod_freetdm-install: mod_freetdm
	cd mod_freetdm && make install

mod_freetdm-clean:
	@if [ -f mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN) ] ; then cd mod_freetdm && make clean ; fi

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(PREFIX)
	$(mkinstalldirs) $(DESTDIR)@confdir@
	@[ -f "$(DESTDIR)@confdir@/freetdm.conf" ] || ( cp conf/*.conf $(DESTDIR)@confdir@)
	@echo OpenFTDM Installed
