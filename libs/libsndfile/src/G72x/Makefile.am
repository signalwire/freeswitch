## Process this file with automake to produce Makefile.in

EXTRA_DIST = README README.original ChangeLog

noinst_HEADERS = g72x.h g72x_priv.h
noinst_LTLIBRARIES = libg72x.la

noinst_PROGRAMS = g72x_test

CFILES = g72x.c g721.c g723_16.c g723_24.c g723_40.c

libg72x_la_SOURCES = $(CFILES) $(noinst_HEADERS)

g72x_test_SOURCES = g72x_test.c
g72x_test_LDADD = ./libg72x.la -lm

check: g72x_test
	./g72x_test all

# Disable autoheader.
AUTOHEADER=echo

## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch 
## revision control system.
##
## arch-tag: d417a8e8-da7f-423d-884d-f03c93379348

