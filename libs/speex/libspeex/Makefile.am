# Disable automatic dependency tracking if using other tools than gcc and gmake
#AUTOMAKE_OPTIONS = no-dependencies


EXTRA_DIST=testenc.c testenc_wb.c testenc_uwb.c testdenoise.c testecho.c

INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir)

lib_LTLIBRARIES = libspeex.la

# Sources for compilation in the library
libspeex_la_SOURCES = nb_celp.c 	sb_celp.c 	lpc.c 	ltp.c 	lsp.c 	quant_lsp.c \
				lsp_tables_nb.c 	gain_table.c 	gain_table_lbr.c 	cb_search.c 	filters.c 	bits.c \
				modes.c 	speex.c 	vq.c 	high_lsp_tables.c 	vbr.c 	hexc_table.c \
				exc_5_256_table.c 	exc_5_64_table.c 	exc_8_128_table.c 	exc_10_32_table.c \
				exc_10_16_table.c 	exc_20_32_table.c 	hexc_10_32_table.c 	misc.c 	speex_header.c \
				speex_callbacks.c 	math_approx.c 	stereo.c 	preprocess.c 	smallft.c 	lbr_48k_tables.c \
				jitter.c 	mdf.c vorbis_psy.c fftwrap.c kiss_fft.c _kiss_fft_guts.h kiss_fft.h \
	kiss_fftr.c kiss_fftr.h window.c

noinst_HEADERS = lsp.h 	nb_celp.h 	lpc.h 	lpc_bfin.h 	ltp.h 	quant_lsp.h \
				cb_search.h 	filters.h 	stack_alloc.h 	vq.h 	vq_sse.h 	vq_arm4.h 	vq_bfin.h \
				modes.h 	sb_celp.h 	vbr.h 	misc.h 	misc_bfin.h 	ltp_sse.h 	ltp_arm4.h \
				ltp_bfin.h 	filters_sse.h 	filters_arm4.h 	filters_bfin.h 	math_approx.h \
				smallft.h 	arch.h 	fixed_arm4.h 	fixed_arm5e.h 	fixed_bfin.h 	fixed_debug.h \
				fixed_generic.h 	cb_search_sse.h 	cb_search_arm4.h 	cb_search_bfin.h vorbis_psy.h \
		fftwrap.h pseudofloat.h lsp_bfin.h quant_lsp_bfin.h


libspeex_la_LDFLAGS = -no-undefined -version-info @SPEEX_LT_CURRENT@:@SPEEX_LT_REVISION@:@SPEEX_LT_AGE@

noinst_PROGRAMS = testenc testenc_wb testenc_uwb testdenoise testecho
testenc_SOURCES = testenc.c
testenc_LDADD = $(top_builddir)/libspeex/libspeex.la
testenc_wb_SOURCES = testenc_wb.c
testenc_wb_LDADD = $(top_builddir)/libspeex/libspeex.la
testenc_uwb_SOURCES = testenc_uwb.c
testenc_uwb_LDADD = $(top_builddir)/libspeex/libspeex.la
testdenoise_SOURCES = testdenoise.c
testdenoise_LDADD = $(top_builddir)/libspeex/libspeex.la
testecho_SOURCES = testecho.c
testecho_LDADD = $(top_builddir)/libspeex/libspeex.la
