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

INCS      = -I$(OZ_SRCDIR)/$(SRC)/include -I$(OZ_SRCDIR)/$(SRC)/isdn/include
if HAVE_SCTP
INCS +=  -I$(OZ_SRCDIR)/$(SRC)/ozmod/ozmod_sangoma_boost 
endif
MY_CFLAGS = $(INCS) $(ZAP_CFLAGS) -DZAP_CONFIG_DIR=\"@confdir@\" -DZAP_MOD_DIR=\"$(moddir)\" @COMP_VENDOR_CFLAGS@ @DEFS@
COMPILE   = $(CC) $(MY_CFLAGS) $(INCS)
LTCOMPILE = $(LIBTOOL) --mode=compile --tag=CC $(COMPILE)
LINK      = $(LIBTOOL) --mode=link --tag=CC $(CC) $(MY_CFLAGS) $(LDFLAGS) -o $@


#
# GNU pkgconfig file
#
EXTRA_DIST = openzap.pc.in

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = openzap.pc


#
# libopenzap
#
libopenzap_la_SOURCES = \
$(SRC)/hashtable.c \
$(SRC)/hashtable_itr.c \
$(SRC)/zap_io.c \
$(SRC)/zap_config.c \
$(SRC)/zap_callerid.c \
$(SRC)/fsk.c \
$(SRC)/uart.c \
$(SRC)/g711.c \
$(SRC)/libteletone_detect.c \
$(SRC)/libteletone_generate.c \
$(SRC)/zap_buffer.c \
$(SRC)/zap_threadmutex.c \
$(SRC)/zap_dso.c \
$(SRC)/zap_cpu_monitor.c

library_include_HEADERS = \
$(SRC)/include/fsk.h \
$(SRC)/include/g711.h \
$(SRC)/include/hashtable.h \
$(SRC)/include/hashtable_itr.h \
$(SRC)/include/hashtable_private.h \
$(SRC)/include/libteletone_detect.h \
$(SRC)/include/libteletone_generate.h \
$(SRC)/include/libteletone.h \
$(SRC)/include/openzap.h \
$(SRC)/include/sangoma_tdm_api.h \
$(SRC)/include/uart.h \
$(SRC)/include/zap_buffer.h \
$(SRC)/include/zap_config.h \
$(SRC)/include/zap_threadmutex.h \
$(SRC)/include/zap_dso.h \
$(SRC)/include/zap_types.h \
$(SRC)/include/zap_cpu_monitor.h

lib_LTLIBRARIES	       = libopenzap.la
libopenzap_la_CFLAGS   = $(AM_CFLAGS) $(MY_CFLAGS)
libopenzap_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libopenzap_la_LIBADD   = $(LIBS)

MYLIB = libopenzap.la

core: libopenzap.la 
core-install: install-libLTLIBRARIES

#
# tools & test programs
#
noinst_PROGRAMS = testtones detect_tones detect_dtmf testisdn testpri testr2 testanalog testapp testcid
if HAVE_SCTP
noinst_PROGRAMS += testboost
endif

testapp_SOURCES = $(SRC)/testapp.c
testapp_LDADD   = libopenzap.la
testapp_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testcid_SOURCES = $(SRC)/testcid.c
testcid_LDADD   = libopenzap.la
testcid_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testtones_SOURCES = $(SRC)/testtones.c
testtones_LDADD   = libopenzap.la
testtones_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

detect_tones_SOURCES = $(SRC)/detect_tones.c
detect_tones_LDADD   = libopenzap.la
detect_tones_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

detect_dtmf_SOURCES = $(SRC)/detect_dtmf.c
detect_dtmf_LDADD   = libopenzap.la
detect_dtmf_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testisdn_SOURCES = $(SRC)/testisdn.c
testisdn_LDADD   = libopenzap.la
testisdn_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testpri_SOURCES = $(SRC)/testpri.c
testpri_LDADD   = libopenzap.la
testpri_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

testr2_SOURCES = $(SRC)/testr2.c
testr2_LDADD   = libopenzap.la
testr2_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

if HAVE_SCTP
testboost_SOURCES = $(SRC)/testboost.c
testboost_LDADD   = libopenzap.la
testboost_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
endif

testanalog_SOURCES = $(SRC)/testanalog.c
testanalog_LDADD   = libopenzap.la
testanalog_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)

#
# ozmod modules
#
mod_LTLIBRARIES = ozmod_zt.la ozmod_skel.la ozmod_isdn.la ozmod_analog.la ozmod_analog_em.la 


if HAVE_SCTP 
mod_LTLIBRARIES += ozmod_sangoma_boost.la
endif

if LIBSANGOMA
mod_LTLIBRARIES += ozmod_wanpipe.la
endif

if LIBPRI
mod_LTLIBRARIES += ozmod_libpri.la
endif

if OPENR2
mod_LTLIBRARIES += ozmod_r2.la
endif

ozmod_zt_la_SOURCES = $(SRC)/ozmod/ozmod_zt/ozmod_zt.c
ozmod_zt_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_zt_la_LDFLAGS = -module -avoid-version
ozmod_zt_la_LIBADD  = $(MYLIB)

ozmod_skel_la_SOURCES = $(SRC)/ozmod/ozmod_skel/ozmod_skel.c
ozmod_skel_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_skel_la_LDFLAGS = -module -avoid-version
ozmod_skel_la_LIBADD  = $(MYLIB)

if LIBSANGOMA
ozmod_wanpipe_la_SOURCES = $(SRC)/ozmod/ozmod_wanpipe/ozmod_wanpipe.c
ozmod_wanpipe_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS) -D__LINUX__ -I/usr/include/wanpipe
ozmod_wanpipe_la_LDFLAGS = -module -avoid-version -lsangoma
ozmod_wanpipe_la_LIBADD  = $(MYLIB)
endif

ozmod_isdn_la_SOURCES = \
$(SRC)/isdn/EuroISDNStateNT.c \
$(SRC)/isdn/EuroISDNStateTE.c \
$(SRC)/isdn/mfifo.c \
$(SRC)/isdn/Q921.c \
$(SRC)/isdn/Q931api.c \
$(SRC)/isdn/Q931.c \
$(SRC)/isdn/Q931ie.c \
$(SRC)/isdn/Q931mes.c \
$(SRC)/isdn/Q931StateNT.c \
$(SRC)/isdn/Q931StateTE.c \
$(SRC)/isdn/nationalmes.c \
$(SRC)/isdn/nationalStateNT.c \
$(SRC)/isdn/nationalStateTE.c \
$(SRC)/isdn/DMSmes.c \
$(SRC)/isdn/DMSStateNT.c \
$(SRC)/isdn/DMSStateTE.c \
$(SRC)/isdn/5ESSmes.c \
$(SRC)/isdn/5ESSStateNT.c \
$(SRC)/isdn/5ESSStateTE.c \
$(SRC)/isdn/Q932mes.c \
$(SRC)/ozmod/ozmod_isdn/ozmod_isdn.c

ozmod_isdn_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS) -D_GNU_SOURCE
ozmod_isdn_la_LDFLAGS = $(PCAP_LIB_FLAGS) -module -avoid-version
ozmod_isdn_la_LIBADD  = $(MYLIB)

ozmod_analog_la_SOURCES = $(SRC)/ozmod/ozmod_analog/ozmod_analog.c
ozmod_analog_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_analog_la_LDFLAGS = -module -avoid-version
ozmod_analog_la_LIBADD  = $(MYLIB)

ozmod_analog_em_la_SOURCES = $(SRC)/ozmod/ozmod_analog_em/ozmod_analog_em.c
ozmod_analog_em_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_analog_em_la_LDFLAGS = -module -avoid-version
ozmod_analog_em_la_LIBADD  = $(MYLIB)

if HAVE_SCTP
ozmod_sangoma_boost_la_SOURCES = $(SRC)/ozmod/ozmod_sangoma_boost/sangoma_boost_client.c $(SRC)/ozmod/ozmod_sangoma_boost/ozmod_sangoma_boost.c
ozmod_sangoma_boost_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_sangoma_boost_la_LDFLAGS = -module -avoid-version
ozmod_sangoma_boost_la_LIBADD  = $(MYLIB)
endif

if LIBPRI
ozmod_libpri_la_SOURCES = $(SRC)/ozmod/ozmod_libpri/ozmod_libpri.c $(SRC)/ozmod/ozmod_libpri/lpwrap_pri.c
ozmod_libpri_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_libpri_la_LDFLAGS = -module  -avoid-version -lpri
ozmod_libpri_la_LIBADD  = $(MYLIB)
endif

if OPENR2
ozmod_r2_la_SOURCES = $(SRC)/ozmod/ozmod_r2/ozmod_r2.c 
ozmod_r2_la_CFLAGS  = $(AM_CFLAGS) $(MY_CFLAGS)
ozmod_r2_la_LDFLAGS = -module  -avoid-version -lopenr2
ozmod_r2_la_LIBADD  = $(MYLIB)
endif


dox doxygen:
	cd docs && doxygen $(OZ_SRCDIR)/docs/Doxygen.conf

mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN): $(MYLIB) mod_openzap/mod_openzap.c
	cd mod_openzap && make

mod_openzap: mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN)

mod_openzap-install: mod_openzap
	cd mod_openzap && make install

mod_openzap-clean:
	@if [ -f mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN) ] ; then cd mod_openzap && make clean ; fi

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(PREFIX)
	$(mkinstalldirs) $(DESTDIR)@confdir@
	@[ -f "$(DESTDIR)@confdir@/openzap.conf" ] || ( cp conf/*.conf $(DESTDIR)@confdir@)
	@echo OpenZAP Installed
