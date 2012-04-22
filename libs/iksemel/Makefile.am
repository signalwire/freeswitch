##
## Process this file with automake to produce Makefile.in
##

SUBDIRS = include src tools test doc

EXTRA_DIST = HACKING iksemel.pc.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = iksemel.pc

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%y%m%d"`
	mv $(PACKAGE)-`date +"%y%m%d"`.tar.gz ../

ikslite:
	cp include/iksemel.h ../
	cat include/finetune.h src/utility.c src/ikstack.c src/sax.c src/iks.c src/dom.c >../iksemel.c
