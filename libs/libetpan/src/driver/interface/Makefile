top_builddir = ../../..

HEADERS = \
		maildriver.h maildriver_types.h maildriver_types_helper.h \
		maildriver_errors.h \
		mailmessage.h mailmessage_types.h \
		mailstorage.h \
		mailstorage_types.h \
		mailfolder.h


SOURCES = \
		maildriver.c maildriver_types.c \
		maildriver_tools.c \
		mailmessage.c mailmessage_tools.c mailmessage_types.c \
		maildriver_types_helper.c \
		mailstorage.c \
		mailstorage_tools.c \
		mailfolder.c


TARGET = libinterface
INCLUDES = -I$(srcdir)/low-level/imf \
	-I$(srcdir)/low-level/mime \
	-I$(srcdir)/data-types \
	-I../tools

#INCLUDES = -I../tools
#INCLUDES = -I../imf -I../tools -I../mime -I../imap -I../mbox -I../pop3 \
#		-I../nntp -I../mh -I../maildir -I../smtp

include $(top_builddir)/Rules
