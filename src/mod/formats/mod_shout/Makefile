# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LOCAL_CFLAGS=-I$(BASE)/libs/curl/include
LOCAL_LDFLAGS=-lshout -lmp3lame $(BASE)/libs/curl/lib/libcurl.la
LOCAL_OBJS=
include $(BASE)/build/modmake.rules
MP3OBJS=common.o dct64_i386.o decode_ntom.o layer3.o tabinit.o interface.o

%.o:  %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

local_depend: $(MP3OBJS)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install lame-3.97.tar.gz --prefix=$(PREFIX) --disable-shared --with-pic
	MOD_CFLAGS="${ALL_CFLAGS}" MOD_LDFLAGS="${LD_CFLAGS}" MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libshout-2.2.2.tar.gz --disable-shared --with-pic --prefix=$(PREFIX)
	cd $(BASE)/libs/curl && $(MAKE)
