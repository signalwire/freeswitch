top_builddir = ../../../..

HEADERS = \
	mboxdriver.h \
	mboxdriver_cached.h \
	mboxdriver_cached_message.h \
	mboxdriver_message.h \
	mboxdriver_types.h \
	mboxstorage.h


SOURCES = \
	mboxdriver.c \
	mboxdriver_cached.c \
	mboxdriver_cached_message.c \
	mboxdriver_message.c \
	mboxdriver_tools.c \
	mboxstorage.c


TARGET = libmbox
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/mbox \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
