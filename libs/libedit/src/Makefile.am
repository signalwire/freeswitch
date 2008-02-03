
BUILT_SOURCES = vi.h emacs.h common.h fcns.h help.h fcns.c help.c

AHDR= vi.h emacs.h common.h 
ASRC= $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c

vi.h: Makefile $(srcdir)/vi.c
	sh $(srcdir)/makelist -h $(srcdir)/vi.c > $@

emacs.h: Makefile $(srcdir)/emacs.c
	sh $(srcdir)/makelist -h $(srcdir)/emacs.c > $@

common.h: Makefile $(srcdir)/common.c
	sh $(srcdir)/makelist -h $(srcdir)/common.c > $@

fcns.h: Makefile $(AHDR)
	sh $(srcdir)/makelist -fh $(AHDR) > $@

help.h: Makefile $(ASRC)
	sh $(srcdir)/makelist -bh $(ASRC) > $@

fcns.c: Makefile $(AHDR)
	sh $(srcdir)/makelist -fc $(AHDR) > $@

help.c: Makefile $(ASRC)
	sh $(srcdir)/makelist -bc $(ASRC) > $@


CLEANFILES = $(BUILT_SOURCES)

lib_LTLIBRARIES    = libedit.la
libedit_la_SOURCES = chared.c common.c el.c emacs.c hist.c key.c map.c parse.c \
							prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c \
							fgetln.c strlcat.c strlcpy.c unvis.c vis.c tokenizer.c \
							history.c filecomplete.c readline.c chared.h el.h hist.h \
							histedit.h key.h map.h parse.h prompt.h read.h refresh.h \
							search.h sig.h sys.h el_term.h tty.h vis.h filecomplete.h \
							editline/readline.h

EXTRA_DIST = makelist shlib_version
nobase_include_HEADERS = histedit.h editline/readline.h

nodist_libedit_la_SOURCES = $(BUILT_SOURCES)

libedit_la_LDFLAGS = -no-undefined -version-info $(LT_VERSION)

