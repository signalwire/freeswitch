MAINTAINERCLEANFILES   = Makefile.in
CLEANFILES             = uni_revision.h

SUBDIRS                = pkgconfig svnrev

include_HEADERS        = uni_version.h uni_revision.h

uni_revision.h :
	svnrev/svnrev -rsvnrev/svnrev.input -p../ -ouni_revision.h
