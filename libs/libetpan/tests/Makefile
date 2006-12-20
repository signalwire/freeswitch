SHELL       = /bin/sh
RM          = rm -fr
CFLAGS	    = -g -O2 -W -Wall `libetpan-config --cflags`
CC	    = gcc
PROGS	    = smime decrypt pgp frm frm-tree frm-simple readmsg readmsg-simple \
	fetch-attachment compose-msg smtpsend readmsg-uid
LDFLAGS	    = `libetpan-config --libs`
INSTALL     = install -c

prefix=/usr/local
bindir=$(prefix)/bin

FRMCOMMON=frm-common.o option-parser.o

all: $(PROGS)

frm: $(FRMCOMMON) frm.o

frm-tree: $(FRMCOMMON) frm-tree.o

frm-simple: $(FRMCOMMON) frm-simple.o

readmsg: option-parser.o readmsg.o readmsg-common.o

readmsg-secure: option-parser.o readmsg-secure.o \
	readmsg-common.o

readmsg-simple: option-parser.o readmsg-simple.o

readmsg-uid: option-parser.o readmsg-uid.o

fetch-attachment: option-parser.o fetch-attachment.o \
	readmsg-common.o

compose-msg: compose-msg.o

smtpsend: smtpsend.o

# no install
install:

clean:
	$(RM) *.o $(PROGS) *~ .libs
