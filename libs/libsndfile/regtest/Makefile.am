## Process this file with automake to produce Makefile.in

bin_PROGRAMS = sndfile-regtest

noinst_HEADERS = regtest.h

SNDFILEDIR =../src
INCLUDES = -I$(srcdir)/$(SNDFILEDIR) $(OS_SPECIFIC_CFLAGS)

sndfile_regtest_SOURCES = sndfile-regtest.c database.c checksum.c
sndfile_regtest_LDADD = $(SNDFILEDIR)/libsndfile.la $(SQLITE3_LIBS)


## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch
## revision control system.
##
## arch-tag: ac4a9626-49ce-4bb4-9fb6-e43de0a23432
