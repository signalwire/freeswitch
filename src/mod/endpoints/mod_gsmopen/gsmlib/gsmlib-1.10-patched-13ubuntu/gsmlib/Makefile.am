## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: GSM library Makefile
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 5.5.1999
# *************************************************************************

INCLUDES =		-I..

lib_LTLIBRARIES =	libgsmme.la

libgsmme_la_SOURCES =	gsm_phonebook.cc gsm_util.cc gsm_unix_serial.cc \
			gsm_me_ta.cc gsm_at.cc gsm_error.cc gsm_parser.cc \
			gsm_sms.cc gsm_sms_codec.cc gsm_sms_store.cc \
			gsm_event.cc gsm_sorted_phonebook.cc \
			gsm_sorted_sms_store.cc gsm_nls.cc \
			gsm_sorted_phonebook_base.cc gsm_cb.cc

gsmincludedir =		$(includedir)/gsmlib

gsminclude_HEADERS =	gsm_at.h gsm_parser.h gsm_sms.h gsm_unix_serial.h \
			gsm_error.h gsm_phonebook.h gsm_sms_codec.h \
			gsm_util.h gsm_me_ta.h gsm_port.h gsm_sms_store.h \
			gsm_event.h gsm_sorted_phonebook.h \
			gsm_sorted_sms_store.h gsm_map_key.h \
			gsm_sorted_phonebook_base.h gsm_cb.h

noinst_HEADERS =	gsm_nls.h gsm_sysdep.h

libgsmme_la_LDFLAGS =	-version-info $(GSM_VERSION)

EXTRA_DIST =		gsm_win32_serial.h gsm_win32_serial.cc
