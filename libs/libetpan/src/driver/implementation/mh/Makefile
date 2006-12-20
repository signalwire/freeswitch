top_builddir = ../../../..

HEADERS = \
	mhdriver.h \
	mhdriver_cached.h \
	mhdriver_cached_message.h \
	mhdriver_message.h \
	mhdriver_types.h \
	mhstorage.h


SOURCES = \
	mhdriver.c \
	mhdriver_cached.c \
	mhdriver_cached_message.c \
	mhdriver_message.c \
	mhdriver_tools.c \
	mhstorage.c


TARGET = libmh
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/mh \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
