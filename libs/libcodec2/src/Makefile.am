AM_CFLAGS = -I../src -fPIC -Wall -O3 -g
AUTOMAKE_OPTIONS = gnu subdir-objects
NAME = codec2
AM_CPPFLAGS = $(AM_CFLAGS)

D=$(abs_srcdir)/codebook

# lsp quantisers

CODEBOOKS= \
  $D/lsp1.txt \
  $D/lsp2.txt \
  $D/lsp3.txt \
  $D/lsp4.txt \
  $D/lsp5.txt \
  $D/lsp6.txt \
  $D/lsp7.txt \
  $D/lsp8.txt \
  $D/lsp9.txt \
  $D/lsp10.txt

# lspd quantisers

CODEBOOKSD= \
  $D/dlsp1.txt \
  $D/dlsp2.txt \
  $D/dlsp3.txt \
  $D/dlsp4.txt \
  $D/dlsp5.txt \
  $D/dlsp6.txt \
  $D/dlsp7.txt \
  $D/dlsp8.txt \
  $D/dlsp9.txt \
  $D/dlsp10.txt 

# lspd VQ quantisers

CODEBOOKSVQ= \
  $D/lsp1.txt \
  $D/lsp2.txt \
  $D/lsp3.txt \
  $D/lsp4.txt \
  $(top_srcdir)/unittest/lsp45678910.txt

CODEBOOKSJND= \
  $D/lsp1.txt \
  $D/lsp2.txt \
  $D/lsp3.txt \
  $D/lsp4.txt \
  $(top_srcdir)/unittest/lspjnd5-10.txt

CODEBOOKSDT= \
  $D/lspdt1.txt \
  $D/lspdt2.txt \
  $D/lspdt3.txt \
  $D/lspdt4.txt \
  $D/lspdt5.txt \
  $D/lspdt6.txt \
  $D/lspdt7.txt \
  $D/lspdt8.txt \
  $D/lspdt9.txt \
  $D/lspdt10.txt

CODEBOOKSJVM= \
  $D/lspjvm1.txt \
  $D/lspjvm2.txt \
  $D/lspjvm3.txt

CODEBOOKSVQANSSI= \
  $D/lspvqanssi1.txt \
  $D/lspvqanssi2.txt \
  $D/lspvqanssi3.txt \
  $D/lspvqanssi4.txt

CODEBOOKSGE= \
  $D/gecb.txt 

noinst_PROGRAMS = generate_codebook genlspdtcb

codebook.$(OBJEXT): codebook.c
codebookd.$(OBJEXT): codebookd.c
codebookdt.$(OBJEXT): codebookdt.c
codebookvq.$(OBJEXT): codebookvq.c
codebookjnd.$(OBJEXT): codebookjnd.c
codebookjvm.$(OBJEXT): codebookjvm.c
codebookvqanssi.$(OBJEXT): codebookvqanssi.c
codebookge.$(OBJEXT): codebookge.c

codebook.lo: codebook.c

codebook.c: generate_codebook $(CODEBOOKS)
	./generate_codebook lsp_cb $(CODEBOOKS) > codebook.c

codebookd.c: generate_codebook $(CODEBOOKSD)
	./generate_codebook lsp_cbd $(CODEBOOKSD) > codebookd.c

codebookdt.c: generate_codebook $(CODEBOOKSDT)
	./generate_codebook lsp_cbdt $(CODEBOOKSDT) > codebookdt.c

codebookvq.c: generate_codebook $(CODEBOOKSVQ)
	./generate_codebook lsp_cbvq $(CODEBOOKSVQ) > codebookvq.c

codebookjnd.c: generate_codebook $(CODEBOOKSJND)
	./generate_codebook lsp_cbjnd $(CODEBOOKSJND) > codebookjnd.c

codebookjvm.c: generate_codebook $(CODEBOOKSJVM)
	./generate_codebook lsp_cbjvm $(CODEBOOKSJVM) > codebookjvm.c

codebookvqanssi.c: generate_codebook $(CODEBOOKSVQANSSI)
	./generate_codebook lsp_cbvqanssi $(CODEBOOKSVQANSSI) > codebookvqanssi.c

codebookge.c: generate_codebook $(CODEBOOKSGE)
	./generate_codebook ge_cb $(CODEBOOKSGE) > codebookge.c

clean-local:
	-rm -f codebook.c codebookd.c codebookdvq.c codebookjnd.c codebookdt.c codebookjvm.c codebookge.c codebookvqanssi.c

lib_LTLIBRARIES	= libcodec2.la
libcodec2_la_SOURCES = dump.c \
lpc.c \
nlp.c \
postfilter.c \
sine.c \
codec2.c \
fifo.c \
fdmdv.c \
kiss_fft.c \
interp.c \
lsp.c \
phase.c \
quantise.c \
pack.c \
codebook.c \
codebookd.c \
codebookvq.c \
codebookjnd.c \
codebookjvm.c \
codebookvqanssi.c \
codebookdt.c \
codebookge.c 


libcodec2_la_CFLAGS = $(AM_CFLAGS)
libcodec2_la_LDFLAGS = $(LIBS)

library_includedir = $(prefix)/include
library_include_HEADERS	= codec2.h

bin_PROGRAMS = c2demo c2enc c2dec c2sim fdmdv_get_test_bits fdmdv_mod fdmdv_demod fdmdv_put_test_bits fdmdv_interleave

c2demo_SOURCES = c2demo.c
c2demo_LDADD = $(lib_LTLIBRARIES) 
c2demo_LDFLAGS = $(LIBS)

c2enc_SOURCES = c2enc.c
c2enc_LDADD = $(lib_LTLIBRARIES) 
c2enc_LDFLAGS = $(LIBS)

c2dec_SOURCES = c2dec.c
c2dec_LDADD = $(lib_LTLIBRARIES) 
c2dec_LDFLAGS = $(LIBS)

c2sim_SOURCES = c2sim.c ampexp.c phaseexp.c
c2sim_LDADD = $(lib_LTLIBRARIES) 
c2sim_LDFLAGS = $(LIBS)

fdmdv_get_test_bits_SOURCES = fdmdv_get_test_bits.c fdmdv.c kiss_fft.c
fdmdv_get_test_bits_LDFLAGS = $(LIBS)

fdmdv_mod_SOURCES = fdmdv_mod.c fdmdv.c kiss_fft.c
fdmdv_mod_LDFLAGS = $(LIBS)

fdmdv_demod_SOURCES = fdmdv_demod.c fdmdv.c kiss_fft.c octave.c
fdmdv_demod_LDFLAGS = $(LIBS)

fdmdv_put_test_bits_SOURCES = fdmdv_put_test_bits.c fdmdv.c kiss_fft.c
fdmdv_put_test_bits_LDFLAGS = $(LIBS)

fdmdv_interleave_SOURCES = fdmdv_interleave.c
fdmdv_interleave_LDFLAGS = $(LIBS)

