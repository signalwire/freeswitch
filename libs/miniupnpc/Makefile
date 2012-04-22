# $Id: Makefile,v 1.48 2008/12/18 17:46:13 nanard Exp $
# MiniUPnP Project
# http://miniupnp.free.fr/
# (c) 2005-2007 Thomas Bernard
# to install use :
# $ PREFIX=/tmp/dummylocation make install
# or
# $ INSTALLPREFIX=/usr/local make install
# or 
# make install (will go to /usr/bin, /usr/lib, etc...)
CC ?= gcc
#AR = gar
#CFLAGS = -fPIC -O -Wall -g -DDEBUG
CFLAGS ?= -fPIC -O -Wall -DNDEBUG
INSTALL = install
#following libs are needed on Solaris
#LDLIBS=-lsocket -lnsl -lresolv

# APIVERSION is used to build SONAME
APIVERSION = 4

SRCS = igd_desc_parse.c miniupnpc.c minixml.c minisoap.c miniwget.c \
       upnpc.c upnpcommands.c upnpreplyparse.c testminixml.c \
	   minixmlvalid.c testupnpreplyparse.c minissdpc.c \
	   upnperrors.c testigddescparse.c

LIBOBJS = miniwget.o minixml.o igd_desc_parse.o minisoap.o \
          miniupnpc.o upnpreplyparse.o upnpcommands.o minissdpc.o \
		  upnperrors.o

OBJS = $(patsubst %.c,%.o,$(SRCS))

# HEADERS to install
HEADERS = miniupnpc.h miniwget.h upnpcommands.h igd_desc_parse.h \
          upnpreplyparse.h upnperrors.h declspec.h
LIBRARY = libminiupnpc.a
SHAREDLIBRARY = libminiupnpc.so
SONAME = $(SHAREDLIBRARY).$(APIVERSION)
EXECUTABLES = upnpc-static upnpc-shared \
              testminixml minixmlvalid testupnpreplyparse \
			  testigddescparse

INSTALLPREFIX ?= $(PREFIX)/usr
INSTALLDIRINC = $(INSTALLPREFIX)/include/miniupnpc
INSTALLDIRLIB = $(INSTALLPREFIX)/lib
INSTALLDIRBIN = $(INSTALLPREFIX)/bin

.PHONY:	install clean depend all installpythonmodule

all:	validateminixml $(LIBRARY) $(EXECUTABLES)

pythonmodule:	$(LIBRARY) miniupnpcmodule.c setup.py
	python setup.py build
	touch $@

installpythonmodule:	pythonmodule
	python setup.py install

validateminixml:	minixmlvalid
	@echo "minixml validation test"
	./minixmlvalid
	touch $@

clean:
		$(RM) $(LIBRARY) $(SHAREDLIBRARY) $(EXECUTABLES) $(OBJS)
		# clean python stuff
		$(RM) pythonmodule validateminixml
		$(RM) -r build/ dist/
		#python setup.py clean

install:	$(LIBRARY) $(SHAREDLIBRARY)
	$(INSTALL) -d $(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(INSTALLDIRINC)
	$(INSTALL) -d $(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(SHAREDLIBRARY) $(INSTALLDIRLIB)/$(SONAME)
	$(INSTALL) -d $(INSTALLDIRBIN)
	$(INSTALL) -m 755 upnpc-shared $(INSTALLDIRBIN)/upnpc
	ln -fs $(SONAME) $(INSTALLDIRLIB)/$(SHAREDLIBRARY)

cleaninstall:
	$(RM) -r $(INSTALLDIRINC)
	$(RM) $(INSTALLDIRLIB)/$(LIBRARY)
	$(RM) $(INSTALLDIRLIB)/$(SHAREDLIBRARY)

depend:
	makedepend -Y -- $(CFLAGS) -- $(SRCS) 2>/dev/null

$(LIBRARY):	$(LIBOBJS)
	$(AR) crs $@ $?

$(SHAREDLIBRARY):	$(LIBOBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^

upnpc-static:	upnpc.o $(LIBRARY)
	$(CC) -o $@ $^

upnpc-shared:	upnpc.o $(SHAREDLIBRARY)
	$(CC) -o $@ $^

testminixml:	minixml.o igd_desc_parse.o testminixml.o

minixmlvalid:	minixml.o minixmlvalid.o

testupnpreplyparse:	testupnpreplyparse.o minixml.o upnpreplyparse.o

testigddescparse:	testigddescparse.o igd_desc_parse.o minixml.o

# DO NOT DELETE THIS LINE -- make depend depends on it.

igd_desc_parse.o: igd_desc_parse.h
miniupnpc.o: miniupnpc.h declspec.h igd_desc_parse.h minissdpc.h miniwget.h
miniupnpc.o: minisoap.h minixml.h upnpcommands.h upnpreplyparse.h
minixml.o: minixml.h
minisoap.o: minisoap.h miniupnpcstrings.h
miniwget.o: miniupnpc.h declspec.h igd_desc_parse.h miniupnpcstrings.h
upnpc.o: miniwget.h declspec.h miniupnpc.h igd_desc_parse.h upnpcommands.h
upnpc.o: upnpreplyparse.h upnperrors.h
upnpcommands.o: upnpcommands.h upnpreplyparse.h declspec.h miniupnpc.h
upnpcommands.o: igd_desc_parse.h
upnpreplyparse.o: upnpreplyparse.h minixml.h
testminixml.o: minixml.h igd_desc_parse.h
minixmlvalid.o: minixml.h
testupnpreplyparse.o: upnpreplyparse.h
minissdpc.o: minissdpc.h miniupnpc.h declspec.h igd_desc_parse.h codelength.h
upnperrors.o: upnperrors.h declspec.h upnpcommands.h upnpreplyparse.h
testigddescparse.o: igd_desc_parse.h minixml.h
