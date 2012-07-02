BASE=../../../..

PKG_DIR:=/usr/local/lib/pkgconfig
ifeq ($(PKG_CONFIG_PATH),)
  export PKG_CONFIG_PATH:=$(PKG_DIR)
else
  ifeq ($(findstring $(PKG_DIR),$(PKG_CONFIG_PATH)),)
    export PKG_CONFIG_PATH:=$(PKG_CONFIG_PATH):$(PKG_DIR)
  endif
endif

#DEBUG_SUFFIX:=--define-variable=suffix=_d

LOCAL_INSERT_CFLAGS= pkg-config opal $(DEBUG_SUFFIX) --cflags 
LOCAL_CFLAGS+=-g -ggdb
LOCAL_INSERT_LDFLAGS= pkg-config opal $(DEBUG_SUFFIX) --libs

include $(BASE)/build/modmake.rules


