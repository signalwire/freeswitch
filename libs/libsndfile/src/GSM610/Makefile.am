## Process this file with automake to produce Makefile.in

EXTRA_DIST = README COPYRIGHT ChangeLog

noinst_HEADERS = gsm.h config.h gsm610_priv.h
noinst_LTLIBRARIES = libgsm.la

CFILES = add.c decode.c gsm_decode.c gsm_encode.c long_term.c preprocess.c \
	short_term.c code.c gsm_create.c gsm_destroy.c gsm_option.c lpc.c rpe.c table.c

libgsm_la_SOURCES = $(CFILES) $(noinst_HEADERS)

# Disable autoheader. 
AUTOHEADER=echo

## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch 
## revision control system.
##
## arch-tag: ba91ffbe-9d1d-4044-a1de-e8ee2f890560

