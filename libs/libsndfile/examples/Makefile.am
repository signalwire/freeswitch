## Process this file with automake to produce Makefile.in

bin_PROGRAMS = sndfile-info sndfile-play sndfile-convert

noinst_PROGRAMS = make_sine sfprocess list_formats cooledit-fixer generate

# This is the BeOS version of sndfile-play. It needs to be compiled with the C++
# compiler.
EXTRA_DIST = sndfile-play-beos.cpp

OS_SPECIFIC_CFLAGS = @OS_SPECIFIC_CFLAGS@
OS_SPECIFIC_LINKS = @OS_SPECIFIC_LINKS@

SNDFILEDIR =../src
INCLUDES = -I$(srcdir)/$(SNDFILEDIR) $(OS_SPECIFIC_CFLAGS)

sndfile_info_SOURCES = sndfile-info.c 
sndfile_info_LDADD = $(SNDFILEDIR)/libsndfile.la

sndfile_play_SOURCES = sndfile-play.c 
sndfile_play_LDADD = $(SNDFILEDIR)/libsndfile.la $(OS_SPECIFIC_LINKS) $(ALSA_LIBS)

sndfile_convert_SOURCES = sndfile-convert.c 
sndfile_convert_LDADD = $(SNDFILEDIR)/libsndfile.la
 
make_sine_SOURCES = make_sine.c 
make_sine_LDADD = $(SNDFILEDIR)/libsndfile.la

sfprocess_SOURCES = sfprocess.c 
sfprocess_LDADD = $(SNDFILEDIR)/libsndfile.la

list_formats_SOURCES = list_formats.c 
list_formats_LDADD = $(SNDFILEDIR)/libsndfile.la

cooledit_fixer_SOURCES = cooledit-fixer.c 
cooledit_fixer_LDADD = $(SNDFILEDIR)/libsndfile.la

generate_SOURCES = generate.c 
generate_LDADD = $(SNDFILEDIR)/libsndfile.la

## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch 
## revision control system.
##
## arch-tag: faeb8674-e417-4162-9ac9-05f2b8369b57

