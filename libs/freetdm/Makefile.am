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

PREFIX=$(prefix)

SRC=src
top_builddir=@oz_builddir@
LTINSTALL=$(LIBTOOL) --mode=install $(INSTALL)
LTUNINSTALL=$(LIBTOOL) --mode=uninstall rm -f 
CCLD = $(CC)
INCS=-I$(OZ_SRCDIR)/$(SRC)/include -I$(OZ_SRCDIR)/$(SRC)/isdn/include -I$(OZ_SRCDIR)/$(SRC)/ozmod/ozmod_ss7_boost 
MY_CFLAGS = $(INCS) $(ZAP_CFLAGS) -DZAP_CONFIG_DIR=\"@prefix@/conf\" -DZAP_MOD_DIR=\"@prefix@/mod\" @COMP_VENDOR_CFLAGS@ @DEFS@

COMPILE      = $(CC) $(MY_CFLAGS) $(INCS)
LTCOMPILE = $(LIBTOOL) --mode=compile --tag=CC $(COMPILE)
LINK = $(LIBTOOL) --mode=link --tag=CC $(CCLD) $(MY_CFLAGS) $(LDFLAGS) -o $@
libdir = $(PREFIX)/lib/

library_includedir      = $(PREFIX)/include

libopenzap_la_SOURCES	= \
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
$(SRC)/zap_dso.c

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
$(SRC)/include/zap_types.h 

BOOST_SOURCE=\
$(SRC)/ozmod/ozmod_ss7_boost/ss7_boost_client.c \
$(SRC)/ozmod/ozmod_ss7_boost/ozmod_ss7_boost.c

ISDN_SOURCE=\
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
$(SRC)/isdn/Q932mes.c

BOOST_OBJS=\
$(SRC)/ozmod/ozmod_ss7_boost/ss7_boost_client.o \
$(SRC)/ozmod/ozmod_ss7_boost/ozmod_ss7_boost.o

ISDN_OBJS=\
$(SRC)/isdn/EuroISDNStateNT.o \
$(SRC)/isdn/EuroISDNStateTE.o \
$(SRC)/isdn/mfifo.o \
$(SRC)/isdn/Q921.o \
$(SRC)/isdn/Q931api.o \
$(SRC)/isdn/Q931.o \
$(SRC)/isdn/Q931ie.o \
$(SRC)/isdn/Q931mes.o \
$(SRC)/isdn/Q931StateNT.o \
$(SRC)/isdn/Q931StateTE.o \
$(SRC)/isdn/nationalmes.o \
$(SRC)/isdn/nationalStateNT.o \
$(SRC)/isdn/nationalStateTE.o \
$(SRC)/isdn/DMSmes.o \
$(SRC)/isdn/DMSStateNT.o \
$(SRC)/isdn/DMSStateTE.o \
$(SRC)/isdn/5ESSmes.o \
$(SRC)/isdn/5ESSStateNT.o \
$(SRC)/isdn/5ESSStateTE.o \
$(SRC)/isdn/Q932mes.o

ISDN_HEADER=\
$(SRC)/isdn/include/mfifo.h \
$(SRC)/isdn/include/national.h \
$(SRC)/isdn/include/DMS.h \
$(SRC)/isdn/include/5ESS.h \
$(SRC)/isdn/include/Q921.h \
$(SRC)/isdn/include/Q931.h \
$(SRC)/isdn/include/Q931ie.h \
$(SRC)/isdn/include/Q932.h 

lib_LTLIBRARIES	       = libopenzap.la
libopenzap_la_CFLAGS   = $(AM_CFLAGS) $(MY_CFLAGS)
libopenzap_la_LDFLAGS  = -version-info 1:0:0 $(AM_LDFLAGS)
libopenzap_la_LIBADD     = $(LIBS)

MYLIB=libopenzap.la

PIKA_DIR=$(shell ls -d /usr/include/pika 2>/dev/null)
PIKA_LIB=$(shell ls /usr/lib/libpikahmpapi.so 2>/dev/null)

all: $(MYLIB) analogmod analog_emmod isdnmod boostmod ztmod wpmod


testapp: $(SRC)/testapp.o $(MYLIB)
	$(LINK) $(SRC)/testapp.o ./$(MYLIB) $(LIBS)

testcid: $(SRC)/testcid.o $(MYLIB)
	$(LINK) $(SRC)/testcid.o ./$(MYLIB) $(LIBS)

testtones: $(SRC)/testtones.o $(MYLIB)
	$(LINK) $(SRC)/testtones.o ./$(MYLIB) $(LIBS)

detect_tones: $(SRC)/detect_tones.o $(MYLIB)
	$(LINK) $(SRC)/detect_tones.o ./$(MYLIB) $(LIBS)

detect_dtmf: $(SRC)/detect_dtmf.o $(MYLIB)
	$(LINK) $(SRC)/detect_dtmf.o ./$(MYLIB) $(LIBS)

testisdn: $(SRC)/testisdn.o $(MYLIB)
	$(LINK) $(SRC)/testisdn.o ./$(MYLIB) $(LIBS)

testboost: $(SRC)/testboost.o $(MYLIB)
	$(LINK) $(SRC)/testboost.o ./$(MYLIB) $(LIBS)

testanalog: $(SRC)/testanalog.o $(MYLIB)
	$(LINK) $(SRC)/testanalog.o ./$(MYLIB) $(LIBS)

$(SRC)/zap_io.o: $(SRC)/zap_io.c
	$(LTCOMPILE) $(MOD_CFLAGS) -c $< -o $@

%.o: %.c $(HEADER)
	$(LTCOMPILE) -c $< -o $@

dox:
	cd docs && doxygen $(OZ_SRCDIR)/docs/Doxygen.conf

mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN): $(MYLIB) mod_openzap/mod_openzap.c
	cd mod_openzap && make

mod_openzap: mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN)

mod_openzap-install: mod_openzap
	cd mod_openzap && make install

mod_openzap-clean:
	@if [ -f mod_openzap/mod_openzap.$(DYNAMIC_LIB_EXTEN) ] ; then cd mod_openzap && make clean ; fi

clean: 
	rm -f $(SRC)/*.o $(SRC)/isdn/*.o $(MYLIB) *~ \#* testapp testcid testtones detect_tones detect_dtmf testisdn testboost testanalog src/ozmod/*/*.o src/ozmod/*/*.$(DYNAMIC_LIB_EXTEN)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(PREFIX)
	$(mkinstalldirs) $(DESTDIR)$(PREFIX)/conf
	@[ -f "$(DESTDIR)$(PREFIX)/conf/openzap.conf" ] || ( cp conf/*.conf $(DESTDIR)$(PREFIX)/conf)
	$(mkinstalldirs)  $(DESTDIR)$(PREFIX)/lib
	$(mkinstalldirs)  $(DESTDIR)$(PREFIX)/mod
	@[ ! -f ozmod_* ] || cp -f ozmod_* $(DESTDIR)$(PREFIX)/mod
	@cp -f src/ozmod/*/*.$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
	@echo OpenZAP Installed

uninstall: oz-uninstall

oz-uninstall: 
	@rm -fr $(DESTDIR)$(PREFIX)

$(SRC)/ozmod/ozmod_skel/ozmod_skel.$(DYNAMIC_LIB_EXTEN): $(SRC)/ozmod/ozmod_skel/ozmod_skel.o $(MYLIB)
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_skel/ozmod_skel.o $(MYLIB) -rpath $(libdir)
skelmod: $(SRC)/ozmod/ozmod_skel/ozmod_skel.$(DYNAMIC_LIB_EXTEN)


$(SRC)/ozmod/ozmod_zt/ozmod_zt.$(DYNAMIC_LIB_EXTEN): $(SRC)/ozmod/ozmod_zt/ozmod_zt.o $(MYLIB)
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_zt/ozmod_zt.o $(MYLIB) -rpath $(libdir)
ztmod: $(SRC)/ozmod/ozmod_zt/ozmod_zt.$(DYNAMIC_LIB_EXTEN)

$(SRC)/ozmod/ozmod_wanpipe/ozmod_wanpipe.$(DYNAMIC_LIB_EXTEN): $(SRC)/ozmod/ozmod_wanpipe/ozmod_wanpipe.o $(MYLIB)
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_wanpipe/ozmod_wanpipe.o $(MYLIB) -rpath $(libdir)
wpmod: $(SRC)/ozmod/ozmod_wanpipe/ozmod_wanpipe.$(DYNAMIC_LIB_EXTEN)

$(SRC)/ozmod/ozmod_pika/ozmod_pika.$(DYNAMIC_LIB_EXTEN): $(SRC)/ozmod/ozmod_pika/ozmod_pika.o $(MYLIB)
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_pika/ozmod_pika.o  $(MYLIB) -rpath $(libdir)
pikamod: $(SRC)/ozmod/ozmod_pika/ozmod_pika.$(DYNAMIC_LIB_EXTEN)

###For libpcap
ISDN_CFLAGS = -D_GNU_SOURCE
####

###For libpcap
ISDN_LIBS = -lpcap
####

$(ISDN_OBJS): $(ISDN_HEADER) $(ISDN_SOURCE) $(SRC)/ozmod/ozmod_isdn/ozmod_isdn.c
$(SRC)/ozmod/ozmod_isdn/ozmod_isdn.o: $(SRC)/ozmod/ozmod_isdn/ozmod_isdn.c
	$(LTCOMPILE) -c $< -o $@ $(ISDN_CFLAGS)

$(SRC)/ozmod/ozmod_isdn/ozmod_isdn.$(DYNAMIC_LIB_EXTEN): $(MYLIB) $(ISDN_OBJS) $(SRC)/ozmod/ozmod_isdn/ozmod_isdn.o
	$(LINK) $(SOLINK) $(ISDN_OBJS) $(SRC)/ozmod/ozmod_isdn/ozmod_isdn.o $(MYLIB) $(ISDN_LIBS) -rpath $(libdir)
isdnmod: $(SRC)/ozmod/ozmod_isdn/ozmod_isdn.$(DYNAMIC_LIB_EXTEN)

$(ISDN_OBJS2): $(ISDN_HEADER) $(ISDN_SOURCE) $(SRC)/ozmod/ozmod_isdn_libpcap/ozmod_isdn.c

$(SRC)/ozmod/ozmod_analog/ozmod_analog.$(DYNAMIC_LIB_EXTEN): $(MYLIB) $(SRC)/ozmod/ozmod_analog/ozmod_analog.o
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_analog/ozmod_analog.o $(MYLIB) -rpath $(libdir)
analogmod: $(SRC)/ozmod/ozmod_analog/ozmod_analog.$(DYNAMIC_LIB_EXTEN)

$(SRC)/ozmod/ozmod_analog_em/ozmod_analog_em.$(DYNAMIC_LIB_EXTEN): $(MYLIB) $(ANALOG_EM_OBJS) $(SRC)/ozmod/ozmod_analog_em/ozmod_analog_em.o
	$(LINK) $(SOLINK) $(SRC)/ozmod/ozmod_analog_em/ozmod_analog_em.o $(MYLIB) -rpath $(libdir)
analog_emmod: $(SRC)/ozmod/ozmod_analog_em/ozmod_analog_em.$(DYNAMIC_LIB_EXTEN)

$(SRC)/ozmod/ozmod_ss7_boost/ozmod_ss7_boost.$(DYNAMIC_LIB_EXTEN): $(MYLIB) $(BOOST_OBJS)
	$(LINK) $(SOLINK) $(BOOST_OBJS) $(MYLIB) -rpath $(libdir)
boostmod: $(SRC)/ozmod/ozmod_ss7_boost/ozmod_ss7_boost.$(DYNAMIC_LIB_EXTEN)


