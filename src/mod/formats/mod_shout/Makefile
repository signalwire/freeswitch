LAME=lame-3.97
SHOUT=libshout-2.2.2
MPG123=mpg123
BASE=../../../..

WANT_CURL=yes

LAME_DIR=$(switch_srcdir)/libs/$(LAME)
SHOUT_DIR=$(switch_srcdir)/libs/$(SHOUT)
MPG123_DIR=$(switch_srcdir)/libs/$(MPG123)

LAME_BUILDDIR=$(switch_builddir)/libs/$(LAME)
SHOUT_BUILDDIR=$(switch_builddir)/libs/$(SHOUT)
MPG123_BUILDDIR=$(switch_builddir)/libs/$(MPG123)

LAMELA=$(LAME_BUILDDIR)/libmp3lame/libmp3lame.la
SHOUTLA=$(SHOUT_BUILDDIR)/src/libshout.la
MPG123LA=$(MPG123_BUILDDIR)/src/libmpg123/libmpg123.la

LOCAL_CFLAGS=-I$(SHOUT_DIR)/include -I$(LAME_DIR)/include -I$(MPG123_DIR)/src  -I$(MPG123_BUILDDIR)/src -I$(SHOUT_BUILDDIR)/include 
LOCAL_LIBADD=$(LAMELA) $(SHOUTLA) $(MPG123LA)
LOCAL_LDFLAGS=`cat $(SHOUT_BUILDDIR)/Makefile | grep SPEEX_LIBS | sed -e 's|^SPEEX_LIBS =||'`  `cat $(SHOUTLA) | grep dependency_libs  | sed -e 's|^dependency_libs=||' | sed -e "s|'||g"`

include $(BASE)/build/modmake.rules

$(MODNAME).lo: $(LOCAL_LIBADD)

$(MPG123_DIR):
	$(GETLIB) $(MPG123).tar.gz

$(MPG123_BUILDDIR)/Makefile: $(MPG123_DIR)
	mkdir -p $(MPG123_BUILDDIR)
	cd $(MPG123_BUILDDIR) && $(DEFAULT_VARS) $(MPG123_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(MPG123_DIR) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(MPG123LA): $(MPG123_BUILDDIR)/Makefile
	cd $(MPG123_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(LAME_DIR):
	$(GETLIB) $(LAME).tar.gz

$(LAME_BUILDDIR)/Makefile: $(LAME_DIR)
	mkdir -p $(LAME_BUILDDIR)
	cd $(LAME_BUILDDIR) && $(DEFAULT_VARS) $(LAME_DIR)/configure $(DEFAULT_ARGS) --disable-decoder --srcdir=$(LAME_DIR) CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(LAMELA): $(LAME_BUILDDIR)/Makefile
	cd $(LAME_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(SHOUT_DIR):
	$(GETLIB) $(SHOUT).tar.gz

$(SHOUT_BUILDDIR)/Makefile: $(SHOUT_DIR)
	mkdir -p $(SHOUT_BUILDDIR)
	cd $(SHOUT_BUILDDIR) && $(DEFAULT_VARS) $(SHOUT_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(SHOUT_DIR) --without-speex --without-vorbis --without-ogg CPPFLAGS= LDFLAGS=
	$(TOUCH_TARGET)

$(SHOUTLA): $(SHOUT_BUILDDIR)/Makefile
	cd $(SHOUT_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)



