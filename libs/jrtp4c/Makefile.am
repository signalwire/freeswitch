EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=gmake
NAME=jrtp4c

AM_CC=g++
AM_CFLAGS	= -Wall -ansi -pedantic -g -O2 -Isrc -I/usr/local/include/jthread -I/usr/local/include/jrtplib3
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libjrtp4c.la
libjrtp4c_la_SOURCES	= src/jrtp4c.cpp
libjrtp4c_la_CFLAGS	= $(AM_CFLAGS)
libjrtp4c_la_LDFLAGS	= -ljrtp -ljthread -lpthread

library_includedir	= $(prefix)/include
library_include_HEADERS = src/jrtp4c.h






