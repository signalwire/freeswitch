top_builddir = ../../../..

HEADERS = \
	pop3driver.h \
	pop3driver_cached.h \
	pop3driver_cached_message.h \
	pop3driver_message.h \
	pop3driver_types.h \
	pop3storage.h


SOURCES = \
	pop3driver.c \
	pop3driver_cached.c \
	pop3driver_cached_message.c \
	pop3driver_message.c \
	pop3driver_tools.c \
	pop3storage.c


TARGET = libpop3
INCLUDES = -I../../interface \
	-I../../tools \
	-I$(srcdir)/low-level/pop3 \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/low-level/imf \
	-I$(srcdir)/data-types


include $(top_builddir)/Rules
