top_builddir = ../..

TARGET = libengine
HEADERS = mailengine.h \
	mailprivacy.h \
	mailprivacy_gnupg.h \
	mailprivacy_smime.h \
	mailprivacy_types.h \
	mailprivacy_tools.h


SOURCES = mailengine.c \
	mailprivacy.c \
	mailprivacy_gnupg.c \
	mailprivacy_smime.c \
	mailprivacy_tools.c

INCLUDES = -I$(srcdir)/driver/interface \
	-I$(srcdir)/driver/implementation/imap

include $(top_builddir)/Rules
