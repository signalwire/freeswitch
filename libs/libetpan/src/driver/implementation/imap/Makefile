top_builddir = ../../../..

HEADERS = \
		imapdriver.h \
		imapdriver_message.h \
		imapdriver_cached.h \
		imapdriver_cached_message.h \
		imapdriver_types.h \
		imapstorage.h


SOURCES = \
		imapdriver.c \
		imapdriver_cached.c \
		imapdriver_cached_message.c \
		imapdriver_message.c \
		imapdriver_tools.c \
		imapstorage.c


TARGET = libimap
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/imap \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
