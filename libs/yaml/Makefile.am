## Run `./bootstrap` to generate the "Makefile.in" files in this directory and
## the "$SUBDIR" subdirectories.

SUBDIRS = include src . tests

EXTRA_DIST = README LICENSE doc/doxygen.cfg

maintainer-clean-local:
	-rm -f aclocal.m4 config.h.in configure config/*
	-find ${builddir} -name Makefile.in -exec rm -f '{}' ';'

.PHONY: bootstrap
bootstrap: maintainer-clean
	./bootstrap

