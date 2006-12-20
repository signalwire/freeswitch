top_builddir = ../..

TARGET = libdata-types
HEADERS = mmapstring.h mailstream.h mailstream_helper.h mail.h \
        mailstream_low.h \
        mailstream_socket.h mailstream_ssl.h \
	mailstream_types.h \
	carray.h clist.h chash.h cinthash.h \
	charconv.h mailsem.h maillock.h
SOURCES = connect.c md5.c mmapstring.c \
	mailstream_helper.c mailstream_low.c \
        mailstream.c mailstream_socket.c mailstream_ssl.c \
	mapping.c cinthash.c carray.c clist.c chash.c \
	charconv.c maillock.c base64.c \
	mail_cache_db.c mailsem.c mailsasl.c

include $(top_builddir)/Rules
