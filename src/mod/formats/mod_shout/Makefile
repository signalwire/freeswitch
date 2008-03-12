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

LOCAL_LDFLAGS=`cat ../../../../libs/libshout-2.2.2/Makefile | grep SPEEX_LIBS | sed -e 's|^SPEEX_LIBS =||'`
MP3SOURCES=common.c dct64_i386.c decode_ntom.c layer3.c tabinit.c interface.c
MP3OBJS=common.o dct64_i386.o decode_ntom.o layer3.o tabinit.o interface.o
LOCAL_OBJS=$(MP3OBJS)
LOCAL_SOURCES=$(MP3SOURCES)

include $(switch_srcdir)/build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(LAME_DIR):
	$(GETLIB) $(LAME).tar.gz

$(LAME_DIR)/Makefile: $(LAME_DIR)
	cd $(LAME_DIR) && ./configure $(DEFAULT_ARGS) --disable-decoder
	$(TOUCH_TARGET)

$(LAMELA): $(LAME_DIR)/Makefile
	cd $(LAME_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(SHOUT_DIR):
	$(GETLIB) $(SHOUT).tar.gz

$(SHOUT_DIR)/Makefile: $(SHOUT_DIR)
	cd $(SHOUT_DIR) && ./configure $(DEFAULT_ARGS) --with-speex=no --without-vorbis
	$(TOUCH_TARGET)

$(SHOUTLA): $(SHOUT_DIR)/Makefile
	cd $(SHOUT_DIR) && $(MAKE)
	$(TOUCH_TARGET)



