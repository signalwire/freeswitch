top_builddir = ../../..

TARGET = libimap
HEADERS = mailimap_helper.h mailimap.h \
        mailimap_types.h mailimap_types_helper.h \
        mailimap_socket.h mailimap_ssl.h \
	mailimap_extension.h mailimap_extension_types.h \
	annotatemore.h annotatemore_types.h \
	acl.h acl_types.h \
	uidplus.h uidplus_types.h

SOURCES = mailimap_types.c mailimap_types_helper.c \
        mailimap_sender.c \
        mailimap_keywords.c \
        mailimap_parser.c \
        mailimap.c \
        mailimap_helper.c \
        mailimap_socket.c \
        mailimap_ssl.c \
	mailimap_print.c \
	mailimap_extension.c \
	annotatemore.c \
	annotatemore_parser.c \
	annotatemore_sender.c \
	annotatemore_types.c \
	acl.c \
	acl_parser.c \
	acl_sender.c \
	acl_types.c \
	uidplus.c \
	uidplus_sender.c \
	uidplus_types.c \
	uidplus_parser.c \


INCLUDES = -I$(srcdir)/data-types

include $(top_builddir)/Rules
