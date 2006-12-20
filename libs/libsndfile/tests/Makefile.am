## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = sfversion floating_point_test write_read_test \
	lossy_comp_test error_test ulaw_test alaw_test dwvw_test \
	peak_chunk_test command_test stdin_test stdout_test stdio_test \
	pcm_test headerless_test pipe_test benchmark header_test misc_test \
	raw_test string_test open_fail_test multi_file_test dither_test \
	scale_clip_test win32_test fix_this aiff_rw_test virtual_io_test \
	locale_test largefile_test win32_ordinal_test cpp_test

SNDFILEDIR = ../src
INCLUDES = -I$(srcdir)/$(SNDFILEDIR)
noinst_HEADERS = dft_cmp.h utils.h

autogen_sources = write_read_test.tpl write_read_test.def	\
					pcm_test.tpl pcm_test.def				\
					header_test.tpl header_test.def			\
					utils.tpl utils.def						\
					scale_clip_test.tpl scale_clip_test.def	\
					pipe_test.tpl pipe_test.def				\
					floating_point_test.tpl floating_point_test.def	\
					benchmark.tpl benchmark.def

EXTRA_DIST = $(autogen_sources)

#===============================================================================

sfversion_SOURCES = sfversion.c
sfversion_LDADD = $(SNDFILEDIR)/libsndfile.la

write_read_test_SOURCES = utils.c write_read_test.c
write_read_test_LDADD = $(SNDFILEDIR)/libsndfile.la

lossy_comp_test_SOURCES = utils.c lossy_comp_test.c
lossy_comp_test_LDADD = $(SNDFILEDIR)/libsndfile.la

fix_this_SOURCES = utils.c fix_this.c
fix_this_LDADD = $(SNDFILEDIR)/libsndfile.la

error_test_SOURCES = error_test.c utils.c
error_test_LDADD = $(SNDFILEDIR)/libsndfile.la

ulaw_test_SOURCES = utils.c ulaw_test.c
ulaw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

alaw_test_SOURCES = utils.c alaw_test.c
alaw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

aiff_rw_test_SOURCES = utils.c aiff_rw_test.c
aiff_rw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

command_test_SOURCES = command_test.c utils.c
command_test_LDADD = $(SNDFILEDIR)/libsndfile.la

locale_test_SOURCES = locale_test.c utils.c
locale_test_LDADD = $(SNDFILEDIR)/libsndfile.la

largefile_test_SOURCES = largefile_test.c utils.c
largefile_test_LDADD = $(SNDFILEDIR)/libsndfile.la

pcm_test_SOURCES = pcm_test.c utils.c
pcm_test_LDADD = $(SNDFILEDIR)/libsndfile.la

headerless_test_SOURCES = utils.c headerless_test.c
headerless_test_LDADD = $(SNDFILEDIR)/libsndfile.la

stdin_test_SOURCES = stdin_test.c utils.c
stdin_test_LDADD = $(SNDFILEDIR)/libsndfile.la

stdout_test_SOURCES = stdout_test.c
stdout_test_LDADD = $(SNDFILEDIR)/libsndfile.la

stdio_test_SOURCES = stdio_test.c utils.c
stdio_test_LDADD = $(SNDFILEDIR)/libsndfile.la

pipe_test_SOURCES = pipe_test.c utils.c
pipe_test_LDADD = $(SNDFILEDIR)/libsndfile.la

benchmark_SOURCES = benchmark.c
benchmark_LDADD = $(SNDFILEDIR)/libsndfile.la

header_test_SOURCES = header_test.c utils.c
header_test_LDADD = $(SNDFILEDIR)/libsndfile.la

misc_test_SOURCES = misc_test.c utils.c
misc_test_LDADD = $(SNDFILEDIR)/libsndfile.la

raw_test_SOURCES = raw_test.c utils.c
raw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

string_test_SOURCES = string_test.c utils.c
string_test_LDADD = $(SNDFILEDIR)/libsndfile.la

dither_test_SOURCES = dither_test.c utils.c
dither_test_LDADD = $(SNDFILEDIR)/libsndfile.la

open_fail_test_SOURCES = open_fail_test.c utils.c
open_fail_test_LDADD = $(SNDFILEDIR)/libsndfile.la

multi_file_test_SOURCES = multi_file_test.c utils.c
multi_file_test_LDADD = $(SNDFILEDIR)/libsndfile.la

virtual_io_test_SOURCES = virtual_io_test.c utils.c
virtual_io_test_LDADD = $(SNDFILEDIR)/libsndfile.la

win32_test_SOURCES = win32_test.c
win32_test_LDADD =

win32_ordinal_test_SOURCES = win32_ordinal_test.c utils.c
win32_ordinal_test_LDADD = $(SNDFILEDIR)/libsndfile.la 

cpp_test_SOURCES = cpp_test.cc utils.c
cpp_test_LDADD = $(SNDFILEDIR)/libsndfile.la 

# Lite remove start
dwvw_test_SOURCES = utils.c dwvw_test.c
dwvw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

floating_point_test_SOURCES = utils.c dft_cmp.c floating_point_test.c
floating_point_test_LDADD = $(SNDFILEDIR)/libsndfile.la

peak_chunk_test_SOURCES = peak_chunk_test.c utils.c
peak_chunk_test_LDADD = $(SNDFILEDIR)/libsndfile.la

scale_clip_test_SOURCES = scale_clip_test.c utils.c
scale_clip_test_LDADD = $(SNDFILEDIR)/libsndfile.la
# Lite remove end

#===============================================================================

write_read_test.c: write_read_test.def write_read_test.tpl
	autogen --writable write_read_test.def

pcm_test.c: pcm_test.def pcm_test.tpl
	autogen --writable pcm_test.def

header_test.c: header_test.def header_test.tpl
	autogen --writable header_test.def

utils.c: utils.def utils.tpl
	autogen --writable utils.def

scale_clip_test.c: scale_clip_test.def scale_clip_test.tpl
	autogen --writable scale_clip_test.def

pipe_test.c: pipe_test.def pipe_test.tpl
	autogen --writable pipe_test.def

floating_point_test.c: floating_point_test.def floating_point_test.tpl
	autogen --writable floating_point_test.def

benchmark.c: benchmark.def benchmark.tpl
	autogen --writable benchmark.def

genfiles : write_read_test.c pcm_test.c header_test.c utils.c \
		scale_clip_test.c pipe_test.c floating_point_test.c benchmark.c

#===============================================================================

check: generic-tests wav-tests aiff-tests au-tests caf-tests raw-tests \
	paf-tests svx-tests nist-tests ircam-tests voc-tests w64-tests mat4-tests \
	mat5-tests pvf-tests xi-tests htk-tests avr-tests sds-tests sd2-tests \
	flac-tests caf-tests io-tests

generic-tests : error_test ulaw_test alaw_test command_test floating_point_test \
		pcm_test win32_ordinal_test cpp_test
	uname -a
	./error_test
	./pcm_test
	./ulaw_test
	./alaw_test
	./dwvw_test
	./command_test ver
	./command_test norm
	./command_test format
	./command_test peak
	./command_test trunc
	./command_test inst
	./command_test bext
	./floating_point_test
	./scale_clip_test
	./headerless_test
	./locale_test
	./win32_ordinal_test
	./cpp_test
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed common tests."
	@echo "----------------------------------------------------------------------"

wav-tests: write_read_test lossy_comp_test peak_chunk_test header_test misc_test multi_file_test
	./write_read_test wav
	./lossy_comp_test wav_ima
	./lossy_comp_test wav_msadpcm
	./lossy_comp_test wav_ulaw
	./lossy_comp_test wav_alaw
	./lossy_comp_test wav_gsm610
	./lossy_comp_test wav_g721
	./peak_chunk_test wav
	./header_test wav
	./misc_test wav
	./string_test wav
	./multi_file_test wav
	./open_fail_test wav
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on WAV files."
	@echo "----------------------------------------------------------------------"

aiff-tests: write_read_test lossy_comp_test peak_chunk_test header_test misc_test aiff_rw_test
	./write_read_test aiff
	./lossy_comp_test aiff_ulaw
	./lossy_comp_test aiff_alaw
	./lossy_comp_test aiff_gsm610
	@echo "=========================="
	@echo "./lossy_comp_test aiff_ima"
	@echo "=========================="
	./peak_chunk_test aiff
	./header_test aiff
	./misc_test aiff
	./string_test aiff
	./multi_file_test aiff
	./aiff_rw_test
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on AIFF files."
	@echo "----------------------------------------------------------------------"

caf-tests: write_read_test lossy_comp_test header_test misc_test
	./write_read_test caf
	./lossy_comp_test caf_ulaw
	./lossy_comp_test caf_alaw
	./header_test caf
	./misc_test caf
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on CAF files."
	@echo "----------------------------------------------------------------------"

au-tests: write_read_test lossy_comp_test header_test misc_test
	./write_read_test au
	./lossy_comp_test au_ulaw
	./lossy_comp_test au_alaw
	./lossy_comp_test au_g721
	./lossy_comp_test au_g723
	./header_test au
	./misc_test au
	./multi_file_test wav
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on AU files."
	@echo "----------------------------------------------------------------------"

raw-tests: write_read_test lossy_comp_test raw_test
	./write_read_test raw
	./lossy_comp_test raw_ulaw
	./lossy_comp_test raw_alaw
	./lossy_comp_test raw_gsm610
	./lossy_comp_test vox_adpcm
	./raw_test
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on RAW (header-less) files."
	@echo "----------------------------------------------------------------------"

w64-tests: write_read_test lossy_comp_test misc_test
	./write_read_test w64
	./lossy_comp_test w64_ima
	./lossy_comp_test w64_msadpcm
	./lossy_comp_test w64_ulaw
	./lossy_comp_test w64_alaw
	./lossy_comp_test w64_gsm610
	./header_test w64
	./misc_test w64
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on W64 files."
	@echo "----------------------------------------------------------------------"

# Lite remove start
paf-tests: write_read_test misc_test
	./write_read_test paf
	./header_test paf
	./misc_test paf
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on PAF files."
	@echo "----------------------------------------------------------------------"

svx-tests: write_read_test misc_test
	./write_read_test svx
	./header_test svx
	./misc_test svx
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on SVX files."
	@echo "----------------------------------------------------------------------"

nist-tests: write_read_test lossy_comp_test misc_test
	./write_read_test nist
	./lossy_comp_test nist_ulaw
	./lossy_comp_test nist_alaw
	./header_test nist
	./misc_test nist
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on NIST files."
	@echo "----------------------------------------------------------------------"

ircam-tests: write_read_test lossy_comp_test misc_test
	./write_read_test ircam
	./lossy_comp_test ircam_ulaw
	./lossy_comp_test ircam_alaw
	./header_test ircam
	./misc_test ircam
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on IRCAM files."
	@echo "----------------------------------------------------------------------"

voc-tests: write_read_test lossy_comp_test misc_test
	./write_read_test voc
	./lossy_comp_test voc_ulaw
	./lossy_comp_test voc_alaw
	./header_test voc
	./misc_test voc
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on VOC files."
	@echo "----------------------------------------------------------------------"

mat4-tests: write_read_test misc_test
	./write_read_test mat4
	./header_test mat4
	./misc_test mat4
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on MAT4 files."
	@echo "----------------------------------------------------------------------"

mat5-tests: write_read_test misc_test
	./write_read_test mat5
	./header_test mat5
	./misc_test mat5
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on MAT5 files."
	@echo "----------------------------------------------------------------------"

pvf-tests: write_read_test misc_test
	./write_read_test pvf
	./header_test pvf
	./misc_test pvf
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on PVF files."
	@echo "----------------------------------------------------------------------"

xi-tests: lossy_comp_test
	./lossy_comp_test xi_dpcm
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on XI files."
	@echo "----------------------------------------------------------------------"

htk-tests: write_read_test misc_test
	./write_read_test htk
	./header_test htk
	./misc_test htk
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on HTK files."
	@echo "----------------------------------------------------------------------"

avr-tests: write_read_test misc_test
	./write_read_test avr
	./header_test avr
	./misc_test avr
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on AVR files."
	@echo "----------------------------------------------------------------------"

sds-tests: write_read_test misc_test
	./write_read_test sds
	./header_test sds
	./misc_test sds
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on SDS files."
	@echo "----------------------------------------------------------------------"

sd2-tests: write_read_test
	./write_read_test sd2
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on SD2 files."
	@echo "----------------------------------------------------------------------"

flac-tests: write_read_test
	./write_read_test flac
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on FLAC files."
	@echo "----------------------------------------------------------------------"

# Lite remove end

io-tests: stdio_test stdin_test stdout_test pipe_test
	./stdio_test
	./pipe_test
	./virtual_io_test
	@echo "----------------------------------------------------------------------"
	@echo "  `./sfversion` passed stdio/pipe/vio tests."
	@echo "----------------------------------------------------------------------"

## Do not edit or modify anything in this comment block.
## The arch-tag line is a file identity tag for the GNU Arch 
## revision control system.
##
## arch-tag: 820ec297-29b9-48a4-998f-48b17053f435

