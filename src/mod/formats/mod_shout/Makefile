# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LAME=lame-3.97
SHOUT=libshout-2.2.2
LAMELA=$(BASE)/libs/$(LAME)/libmp3lame/libmp3lame.la
SHOUTLA=$(BASE)/libs/$(SHOUT)/src/libshout.la
CURLLA=$(BASE)/libs/curl/lib/libcurl.la
LOCAL_CFLAGS=-I$(BASE)/libs/curl/include -I$(BASE)/libs/$(SHOUT)/include -I$(BASE)/libs/$(LAME)/include
LOCAL_LDFLAGS=$(LAMELA) $(SHOUTLA) $(CURLLA)
MP3OBJS=common.o dct64_i386.o decode_ntom.o layer3.o tabinit.o interface.o
LOCAL_OBJS=$(MP3OBJS)
DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

include $(BASE)/build/modmake.rules

$(LAMELA):
	MAKE="$(MAKE)" $(BASE)/build/buildlib.sh $(BASE) $(LAME).tar.gz $(DEFAULT_ARGS) --disable-decoder

$(SHOUTLA):
	MAKE="$(MAKE)" MOD_CFLAGS="${ALL_CFLAGS}" MOD_LDFLAGS="${LD_CFLAGS}" $(BASE)/build/buildlib.sh $(BASE) $(SHOUT).tar.gz $(DEFAULT_ARGS)

$(CURLLA):
	cd $(BASE)/libs/curl && $(MAKE)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

local_depend: $(LAMELA) $(SHOUTLA) $(MP3OBJS)



