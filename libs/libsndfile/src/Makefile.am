## Process this file with automake to produce Makefile.in

SUBDIRS = GSM610 G72x

lib_LTLIBRARIES = libsndfile.la
include_HEADERS = sndfile.hh
nodist_include_HEADERS = sndfile.h

OS_SPECIFIC_CFLAGS = @OS_SPECIFIC_CFLAGS@
OS_SPECIFIC_LINKS = @OS_SPECIFIC_LINKS@

EXTRA_DIST = sndfile.h.in config.h.in test_endswap.tpl test_endswap.def \
			Symbols.linux Symbols.darwin libsndfile.def cygsndfile.def \
			create_symbols_file.py binheader_writef_check.py

noinst_HEADERS = common.h sfconfig.h sfendian.h float_cast.h wav_w64.h sf_unistd.h

noinst_PROGRAMS = test_endswap test_file_io test_log_printf

COMMON = sndfile.c common.c file_io.c command.c pcm.c ulaw.c alaw.c float32.c \
		double64.c ima_adpcm.c ms_adpcm.c gsm610.c dwvw.c vox_adpcm.c \
		interleave.c strings.c dither.c broadcast.c

FILESPECIFIC = aiff.c au.c g72x.c avr.c caf.c ircam.c macbinary3.c macos.c \
				mat4.c mat5.c nist.c paf.c pvf.c raw.c svx.c voc.c htk.c dwd.c \
				ogg.c rx2.c sd2.c sds.c txw.c wve.c w64.c wav_w64.c wav.c xi.c \
				flac.c

# MinGW requires -no-undefined if a DLL is to be built.
libsndfile_la_LDFLAGS = -no-undefined -version-info @SHARED_VERSION_INFO@ @SHLIB_VERSION_ARG@
libsndfile_la_SOURCES = $(COMMON) $(FILESPECIFIC) $(noinst_HEADERS)
nodist_libsndfile_la_SOURCES = $(nodist_include_HEADERS)
libsndfile_la_LIBADD  = GSM610/libgsm.la G72x/libg72x.la -lm $(FLAC_LIBS)

test_endswap_SOURCES = test_endswap.c

test_file_io_CFLAGS = $(AM_CFLAGS)
test_file_io_SOURCES = file_io.c test_file_io.c

test_log_printf_CFLAGS = $(AM_CFLAGS)
test_log_printf_SOURCES = test_log_printf.c

test_endswap.c: test_endswap.def test_endswap.tpl
	autogen --writable test_endswap.def

genfiles : test_endswap.c Symbols.linux Symbols.darwin libsndfile.def cygsndfile.def

# Two test programs.
# It is not possible to place these in the tests/ directory because they
# need access to the internals of the SF_PRIVATE struct.

check: test_endswap test_file_io test_log_printf
	@echo
	@echo
	@echo
	@echo "============================================================"
	@if [ -x /usr/bin/python2.4 ]; then $(srcdir)/binheader_writef_check.py $(srcdir)/*.c ; fi
	./test_endswap
	./test_file_io
	./test_log_printf
	@echo "============================================================"
	@echo
	@echo
	@echo

#======================================================================
# Generate an OS specific Symbols files. This is done when the author
# builds the distribution tarball. There should be not need for the
# end user to create these files.

Symbols.linux: create_symbols_file.py
	./create_symbols_file.py linux $(VERSION) > Symbols.linux

Symbols.darwin: create_symbols_file.py
	./create_symbols_file.py darwin $(VERSION) > Symbols.darwin

libsndfile.def: create_symbols_file.py
	./create_symbols_file.py win32 $(VERSION) > libsndfile.def

cygsndfile.def: create_symbols_file.py
	./create_symbols_file.py cygwin $(VERSION) > cygsndfile.def

# Fake dependancy to force the creation of these files.
sndfile.c : Symbols.linux Symbols.darwin libsndfile.def cygsndfile.def

#======================================================================
# Disable autoheader.
AUTOHEADER=echo

# Dependancies.

aiff.c au.c g72x.c ircam.c mat4.c mat5.c nist.c paf.c pvf.c : sndfile.h common.h
raw.c svx.c voc.c w64.c wav.c wav_w64.c htk.c sd2.c rx2.c txw.c : sndfile.h common.h
sds.c wve.c dwd.c ogg.c xi.c sndfile.c common.c file_io.c : sndfile.h common.h
command.c pcm.c ulaw.c alaw.c float32.c double64.c ima_adpcm.c : sndfile.h common.h
ms_adpcm.c gsm610.c dwvw.c vox_adpcm.c interleave.c strings.c : sndfile.h common.h
dither.c : sndfile.h common.h

## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch
## revision control system.
##
## arch-tag: fc3511e6-4230-4bcb-9c86-f728d7a06fe7

