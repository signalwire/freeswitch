LAME=lame-3.97
SHOUT=libshout-2.2.2
MPG123=mpg123
switch_srcdir=../../../..

WANT_CURL=yes

LAME_DIR=$(switch_srcdir)/libs/$(LAME)
SHOUT_DIR=$(switch_srcdir)/libs/$(SHOUT)
MPG123_DIR=$(switch_srcdir)/libs/$(MPG123)

LAMELA=$(LAME_DIR)/libmp3lame/libmp3lame.la
SHOUTLA=$(SHOUT_DIR)/src/libshout.la
MPG123LA=$(MPG123_DIR)/src/libmpg123/.libs/libmpg123.la

LOCAL_CFLAGS=-I$(SHOUT_DIR)/include -I$(LAME_DIR)/include -I$(MPG123_DIR)/src
LOCAL_LIBADD=$(LAMELA) $(SHOUTLA) $(MPG123LA)

LOCAL_LDFLAGS=`cat ../../../../libs/libshout-2.2.2/Makefile | grep SPEEX_LIBS | sed -e 's|^SPEEX_LIBS =||'` 
LOCAL_OBJS=$(MP3OBJS)
LOCAL_SOURCES=$(MP3SOURCES)

include $(switch_srcdir)/build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

$(LOCAL_OBJS): $(LOCAL_SOURCES)

$(MPG123_DIR):
	$(GETLIB) $(MPG123).tar.gz

$(MPG123_DIR)/Makefile: $(MPG123_DIR)
	cd $(MPG123_DIR) && ./configure --disable-shared --with-pic
	$(TOUCH_TARGET)

$(MPG123LA): $(MPG123_DIR)/Makefile
	cd $(MPG123_DIR) && $(MAKE)
	$(TOUCH_TARGET)

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
	cd $(SHOUT_DIR) && ./configure $(DEFAULT_ARGS) --without-speex --without-vorbis --without-ogg
	$(TOUCH_TARGET)

$(SHOUTLA): $(SHOUT_DIR)/Makefile
	cd $(SHOUT_DIR) && $(MAKE)
	$(TOUCH_TARGET)



