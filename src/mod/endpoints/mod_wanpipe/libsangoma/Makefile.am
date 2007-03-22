AM_CFLAGS		= -fPIC -Wall -Wstrict-prototypes -Wmissing-prototypes -g -I/usr/src/linux/include -I. \
	-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -O2 -D_DEBUG_=2 -D_GNUC_ -DWANPIPE_TDM_API -I../lib -I/usr/src/linux/include
LIB_SOURCES		= libsangoma.c  libsangoma.h

if LIBPRI
LIB_SOURCES		+= sangoma_pri.c sangoma_pri.h
endif 

library_includedir      = $(includedir)
library_include_HEADERS = libsangoma.h 

if LIBPRI
library_include_HEADERS	+= sangoma_pri.h
endif

lib_LTLIBRARIES         = libsangoma.la
libsangoma_la_CFLAGS    = $(AM_CFLAGS)
libsangoma_la_LDFLAGS   = -version-info 1:0:0

if LIBPRI
libsangoma_la_CFLAGS	+= -I$(libpripath)
libsangoma_la_LDFLAGS	+= -L$(libpripath) -lpri
endif 

libsangoma_la_SOURCES   = $(LIB_SOURCES)

lib_LIBRARIES           = libsangoma.a
libsangoma_a_CFLAGS     = $(AM_CFLAGS)
libsangoma_a_SOURCES    = $(LIB_SOUCES)
