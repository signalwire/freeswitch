# Copyright (c) 2007-2012, Anthony Minessale II
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

ACLOCAL_AMFLAGS  = -I build
AUTOMAKE_OPTIONS = foreign

SRC = src

moddir = @modinstdir@
libdir = @libdir@
library_includedir = $(prefix)/include

INCS  = -I$(FT_SRCDIR)/$(SRC)/include -I$(FT_SRCDIR)/$(SRC)/include/private

# we needed to separate CFLAGS in FTDM_COMPAT_CFLAGS and FTDM_CFLAGS due to -c99 which causes problems with wanpipe headers
FTDM_COMPAT_CFLAGS = $(INCS) -DFTDM_CONFIG_DIR=\"@confdir@\" -DFTDM_MOD_DIR=\"$(moddir)\" @COMP_VENDOR_COMPAT_CFLAGS@ @DEFS@
FTDM_CFLAGS = $(INCS) -DFTDM_CONFIG_DIR=\"@confdir@\" -DFTDM_MOD_DIR=\"$(moddir)\" @COMP_VENDOR_CFLAGS@ @DEFS@
COMPILE   = $(CC) $(FTDM_CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile --tag=CC $(COMPILE)
LINK      = $(LIBTOOL) --mode=link --tag=CC $(CC) $(FTDM_CFLAGS) $(LDFLAGS) -o $@


#
# GNU pkgconfig file
#
EXTRA_DIST = freetdm.pc.in

pkgconfigdir   = @pkgconfigdir@
pkgconfig_DATA = freetdm.pc


#
# libfreetdm
#
libfreetdm_la_SOURCES = \
	$(SRC)/hashtable.c \
	$(SRC)/hashtable_itr.c \
	$(SRC)/ftdm_io.c \
	$(SRC)/ftdm_state.c \
	$(SRC)/ftdm_queue.c \
	$(SRC)/ftdm_sched.c \
	$(SRC)/ftdm_call_utils.c \
	$(SRC)/ftdm_variables.c \
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
	$(SRC)/ftdm_cpu_monitor.c \
	$(SRC)/ftdm_backtrace.c

library_include_HEADERS = \
	$(SRC)/include/freetdm.h \
	$(SRC)/include/ftdm_declare.h \
	$(SRC)/include/ftdm_threadmutex.h \
	$(SRC)/include/ftdm_os.h \
	$(SRC)/include/ftdm_call_utils.h \
	$(SRC)/include/ftdm_dso.h 

lib_LTLIBRARIES	       = libfreetdm.la
libfreetdm_la_CFLAGS   = $(FTDM_CFLAGS) $(AM_CFLAGS)
libfreetdm_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libfreetdm_la_LIBADD   = $(LIBS)

core: libfreetdm.la 
core-install: install-libLTLIBRARIES

#
# tools & test programs
#
noinst_PROGRAMS  = testtones detect_tones detect_dtmf testpri testr2 testanalog testapp testcid

testapp_SOURCES = $(SRC)/testapp.c
testapp_LDADD   = libfreetdm.la
testapp_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

testcid_SOURCES = $(SRC)/testcid.c
testcid_LDADD   = libfreetdm.la
testcid_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

testtones_SOURCES = $(SRC)/testtones.c
testtones_LDADD   = libfreetdm.la
testtones_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

detect_tones_SOURCES = $(SRC)/detect_tones.c
detect_tones_LDADD   = libfreetdm.la
detect_tones_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

detect_dtmf_SOURCES = $(SRC)/detect_dtmf.c
detect_dtmf_LDADD   = libfreetdm.la
detect_dtmf_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

#testisdn_SOURCES = $(SRC)/testisdn.c
#testisdn_LDADD   = libfreetdm.la
#testisdn_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

testpri_SOURCES = $(SRC)/testpri.c
testpri_LDADD   = libfreetdm.la
testpri_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

testr2_SOURCES = $(SRC)/testr2.c
testr2_LDADD   = libfreetdm.la
testr2_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

testanalog_SOURCES = $(SRC)/testanalog.c
testanalog_LDADD   = libfreetdm.la
testanalog_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)

#
# ftmod modules
#
mod_LTLIBRARIES = ftmod_zt.la ftmod_skel.la ftmod_analog.la ftmod_analog_em.la

ftmod_zt_la_SOURCES = $(SRC)/ftmod/ftmod_zt/ftmod_zt.c
ftmod_zt_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_zt_la_LDFLAGS = -shared -module -avoid-version
ftmod_zt_la_LIBADD  = libfreetdm.la

ftmod_skel_la_SOURCES = $(SRC)/ftmod/ftmod_skel/ftmod_skel.c
ftmod_skel_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_skel_la_LDFLAGS = -shared -module -avoid-version
ftmod_skel_la_LIBADD  = libfreetdm.la

ftmod_analog_la_SOURCES = $(SRC)/ftmod/ftmod_analog/ftmod_analog.c
ftmod_analog_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_analog_la_LDFLAGS = -shared -module -avoid-version
ftmod_analog_la_LIBADD  = libfreetdm.la

ftmod_analog_em_la_SOURCES = $(SRC)/ftmod/ftmod_analog_em/ftmod_analog_em.c
ftmod_analog_em_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_analog_em_la_LDFLAGS = -shared -module -avoid-version
ftmod_analog_em_la_LIBADD  = libfreetdm.la

if HAVE_LIBSANGOMA
mod_LTLIBRARIES += ftmod_wanpipe.la
ftmod_wanpipe_la_SOURCES = $(SRC)/ftmod/ftmod_wanpipe/ftmod_wanpipe.c
# some structures within Wanpipe drivers are not c99 compatible, so we need to compile ftmod_wanpipe
# without c99 flags, use FTDM_COMPAT_CFLAGS instead
ftmod_wanpipe_la_CFLAGS  = $(FTDM_COMPAT_CFLAGS) $(AM_CFLAGS) -D__LINUX__ -I/usr/include/wanpipe
ftmod_wanpipe_la_LDFLAGS = -shared -module -avoid-version
ftmod_wanpipe_la_LIBADD  = libfreetdm.la -lsangoma
endif

if HAVE_LIBISDN
mod_LTLIBRARIES += ftmod_isdn.la
ftmod_isdn_la_SOURCES  = $(SRC)/ftmod/ftmod_isdn/ftmod_isdn.c
ftmod_isdn_la_CFLAGS   = $(FTDM_CFLAGS) $(AM_CFLAGS) -D_GNU_SOURCE $(LIBISDN_CPPFLAGS) $(PCAP_CPPFLAGS)
ftmod_isdn_la_LDFLAGS  = -shared -module -avoid-version $(LIBISDN_LDFLAGS) $(PCAP_LDFLAGS)
ftmod_isdn_la_LIBADD   = libfreetdm.la $(LIBISDN_LIBS) $(PCAP_LIBS)
endif

if HAVE_LIBPRI
mod_LTLIBRARIES += ftmod_libpri.la
ftmod_libpri_la_SOURCES = $(SRC)/ftmod/ftmod_libpri/ftmod_libpri.c $(SRC)/ftmod/ftmod_libpri/lpwrap_pri.c
ftmod_libpri_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS) $(LIBPRI_CPPFLAGS)
ftmod_libpri_la_LDFLAGS = -shared -module -avoid-version $(LIBPRI_LDFLAGS)
ftmod_libpri_la_LIBADD  = libfreetdm.la $(LIBPRI_LIBS)
endif

if HAVE_PRITAP
mod_LTLIBRARIES += ftmod_pritap.la
ftmod_pritap_la_SOURCES = $(SRC)/ftmod/ftmod_pritap/ftmod_pritap.c 
ftmod_pritap_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_pritap_la_LDFLAGS = -shared -module -avoid-version
ftmod_pritap_la_LIBADD  = libfreetdm.la -lpri
endif

if HAVE_SNG_SS7
mod_LTLIBRARIES += ftmod_sangoma_ss7.la
ftmod_sangoma_ss7_la_SOURCES = \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_support.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_main.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_handle.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_in.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_out.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cntrl.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_xml.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_timers.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cli.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_cfg.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_sta.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_sts.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_logger.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_m2ua_xml.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_m2ua.c \
	$(SRC)/ftmod/ftmod_sangoma_ss7/ftmod_sangoma_ss7_relay.c

ftmod_sangoma_ss7_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS) -D_GNU_SOURCE
ftmod_sangoma_ss7_la_LDFLAGS = -shared -module -avoid-version
ftmod_sangoma_ss7_la_LIBADD  = libfreetdm.la -lsng_ss7
endif

if HAVE_SNG_ISDN
mod_LTLIBRARIES += ftmod_sangoma_isdn.la
ftmod_sangoma_isdn_la_SOURCES = \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_cfg.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_cntrl.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_trace.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_transfer.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_support.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_cntrl.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_cfg.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_rcv.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_hndl.c \
	$(SRC)/ftmod/ftmod_sangoma_isdn/ftmod_sangoma_isdn_stack_out.c

ftmod_sangoma_isdn_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS) -D_GNU_SOURCE
ftmod_sangoma_isdn_la_LDFLAGS = -shared -module -avoid-version
ftmod_sangoma_isdn_la_LIBADD  = libfreetdm.la -lsng_isdn
endif

if HAVE_OPENR2
mod_LTLIBRARIES += ftmod_r2.la
ftmod_r2_la_SOURCES = $(SRC)/ftmod/ftmod_r2/ftmod_r2.c  $(SRC)/ftmod/ftmod_r2/ftmod_r2_io_mf_lib.c
ftmod_r2_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_r2_la_LDFLAGS = -shared -module -avoid-version
ftmod_r2_la_LIBADD  = libfreetdm.la -lopenr2
endif

if HAVE_WAT
mod_LTLIBRARIES += ftmod_gsm.la
ftmod_gsm_la_SOURCES = $(SRC)/ftmod/ftmod_gsm/ftmod_gsm.c 
ftmod_gsm_la_CFLAGS  = $(FTDM_CFLAGS) $(AM_CFLAGS)
ftmod_gsm_la_LDFLAGS = -shared -module -avoid-version
ftmod_gsm_la_LIBADD  = libfreetdm.la -lwat
endif

if HAVE_MISDN
mod_LTLIBRARIES += ftmod_misdn.la
ftmod_misdn_la_SOURCES  = $(SRC)/ftmod/ftmod_misdn/ftmod_misdn.c
ftmod_misdn_la_CFLAGS   = $(FTDM_CFLAGS) $(AM_CFLAGS) $(MISDN_CFLAGS) -D_GNU_SOURCE
ftmod_misdn_la_LDFLAGS  = -shared -module -avoid-version
ftmod_misdn_la_LIBADD   = libfreetdm.la
endif

dox doxygen:
	doxygen $(FT_SRCDIR)/docs/Doxygen.conf

mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN): libfreetdm.la mod_freetdm/mod_freetdm.c
	$(MAKE) -C mod_freetdm

mod_freetdm: mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN)

mod_freetdm-install: mod_freetdm
	$(MAKE) -C mod_freetdm install

mod_freetdm-clean:
	@if [ -f mod_freetdm/mod_freetdm.$(DYNAMIC_LIB_EXTEN) ] ; then \
		$(MAKE) -C mod_freetdm clean ; \
	fi

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(prefix)
	$(mkinstalldirs) $(DESTDIR)@confdir@
	@[ -f "$(DESTDIR)@confdir@/freetdm.conf" ] || ( cp conf/*.conf $(DESTDIR)@confdir@)
	@echo FreeTDM Installed
