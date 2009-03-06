## Process this file with automake to produce Makefile.in. -*-Makefile-*-

# To disable automatic dependency tracking if using other tools than
# gcc and gmake, add the option 'no-dependencies'
AUTOMAKE_OPTIONS = 1.7

m4datadir = $(datadir)/aclocal
m4data_DATA = speex.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = speex.pc speexdsp.pc

EXTRA_DIST = Speex.spec Speex.spec.in Speex.kdevelop speex.m4 speex.pc.in README.blackfin README.symbian README.TI-DSP

#Fools KDevelop into including all files
SUBDIRS = libspeex include

DIST_SUBDIRS = libspeex include src doc win32 symbian ti

rpm: dist
	rpmbuild -ta ${PACKAGE}-${VERSION}.tar.gz
