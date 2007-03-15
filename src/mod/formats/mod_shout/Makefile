MODNAME=mod_shout
LAME=lame-3.97
SHOUT=libshout-2.2.2
switch_srcdir=../../../..

WANT_CURL=yes

LAME_DIR=$(switch_srcdir)/libs/$(LAME)
SHOUT_DIR=$(switch_srcdir)/libs/$(SHOUT)

LAMELA=$(LAME_DIR)/libmp3lame/libmp3lame.la
SHOUTLA=$(SHOUT_DIR)/src/libshout.la

LOCAL_CFLAGS=-I$(SHOUT_DIR)/include -I$(LAME_DIR)/include
LOCAL_LIBADD=$(LAMELA) $(SHOUTLA)

MP3SOURCES=common.c dct64_i386.c decode_ntom.c layer3.c tabinit.c interface.c
MP3OBJS=common.o dct64_i386.o decode_ntom.o layer3.o tabinit.o interface.o
LOCAL_OBJS=$(MP3OBJS)
LOCAL_SOURCES=$(MP3SOURCES)

include $(switch_srcdir)/build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(LAME_DIR)/Makefile:
	$(GETLIB) $(LAME).tar.gz
	cd $(LAME_DIR) && ./configure $(DEFAULT_ARGS) --disable-decoder

$(LAMELA): $(LAME_DIR)/Makefile
	cd $(LAME_DIR) && $(MAKE)

$(SHOUT_DIR)/Makefile:
	$(GETLIB) $(SHOUT).tar.gz
	cd $(SHOUT_DIR) && ./configure $(DEFAULT_ARGS)

$(SHOUTLA): $(SHOUT_DIR)/Makefile
	cd $(SHOUT_DIR) && $(MAKE)




