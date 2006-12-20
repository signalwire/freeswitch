top_builddir = ../../../..

HEADERS = \
	maildirdriver.h \
	maildirdriver_cached.h \
	maildirdriver_cached_message.h \
	maildirdriver_message.h \
	maildirdriver_types.h \
	maildirstorage.h


SOURCES = \
	maildirdriver.c \
	maildirdriver_cached.c \
	maildirdriver_cached_message.c \
	maildirdriver_message.c \
	maildirdriver_tools.c \
	maildirstorage.c


TARGET = libmaildir
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/maildir \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
