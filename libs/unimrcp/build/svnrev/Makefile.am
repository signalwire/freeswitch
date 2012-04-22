MAINTAINERCLEANFILES = Makefile.in

INCLUDES             = $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_PROGRAMS      = svnrev
svnrev_LDADD         = $(UNIMRCP_APR_LIBS) $(UNIMRCP_APU_LIBS)
svnrev_SOURCES       = svnrev.c

rev:
	./svnrev -rsvnrev.input -p../../ -o../uni_revision.h
