# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:
OSARCH=$(shell uname -s)
# and define these variables to impact your build
MODNAME=mod_cepstral

ifeq ($(OSARCH),Darwin)
LOCAL_LDFLAGS += -framework swift
else
LOCAL_LDFLAGS += -L/opt/swift/lib -lswift -lceplex_us -lceplang_en -lm
LOCAL_CFLAGS += -I/opt/swift/include
endif
LOCAL_OBJS=
include ../../../../build/modmake.rules

