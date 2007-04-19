#
# Makefile.am for sofia-sip/utils
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#
# ref: http://www.gnu.org/software/automake/manual/automake.html

# ----------------------------------------------------------------------
# Header paths

sofiasrc=$(top_srcdir)/libsofia-sip-ua
sofiabld=$(top_builddir)/libsofia-sip-ua

INCLUDES = 	-I$(sofiasrc)/features -I$(sofiabld)/features \
		-I$(sofiasrc)/ipt -I$(sofiabld)/ipt \
		-I$(sofiasrc)/iptsec -I$(sofiabld)/iptsec \
		-I$(sofiasrc)/bnf -I$(sofiabld)/bnf \
		-I$(sofiasrc)/http -I$(sofiabld)/http \
		-I$(sofiasrc)/msg -I$(sofiabld)/msg \
		-I$(sofiasrc)/nth -I$(sofiabld)/nth \
		-I$(sofiasrc)/nta -I$(sofiabld)/nta \
		-I$(sofiasrc)/nea -I$(sofiabld)/nea \
		-I$(sofiasrc)/nua -I$(sofiabld)/nua \
		-I$(sofiasrc)/soa -I$(sofiabld)/soa \
		-I$(sofiasrc)/sdp -I$(sofiabld)/sdp \
		-I$(sofiasrc)/sip -I$(sofiabld)/sip \
		-I$(sofiasrc)/soa -I$(sofiabld)/soa \
		-I$(sofiasrc)/sresolv -I$(sofiabld)/sresolv \
		-I$(sofiasrc)/tport -I$(sofiabld)/tport \
		-I$(sofiasrc)/stun -I$(sofiabld)/stun \
		-I$(sofiasrc)/url -I$(sofiabld)/url \
		-I$(sofiasrc)/su -I$(sofiabld)/su


AM_CFLAGS =		$(SOFIA_CFLAGS)

# ----------------------------------------------------------------------
# Build targets

noinst_HEADERS = 	apps_utils.h

bin_PROGRAMS =		sip-options sip-date sip-dig

LDADD =			../libsofia-sip-ua/libsofia-sip-ua.la $(GLIB_LIBS)
