top_builddir = ../../../..

HEADERS = \
	nntpdriver.h \
	nntpdriver_cached.h \
	nntpdriver_cached_message.h \
	nntpdriver_message.h \
	nntpdriver_types.h \
	nntpstorage.h


SOURCES = \
	nntpdriver.c \
	nntpdriver_cached.c \
	nntpdriver_cached_message.c \
	nntpdriver_message.c \
	nntpdriver_tools.c \
	nntpstorage.c


TARGET = libnntp
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/nntp \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
