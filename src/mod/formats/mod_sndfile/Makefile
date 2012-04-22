BASE=../../../..

SNDFILE_DIR=$(switch_srcdir)/libs/libsndfile
SNDFILE_BUILDDIR=$(switch_builddir)/libs/libsndfile
SNDFILELA=$(SNDFILE_BUILDDIR)/src/libsndfile.la

LOCAL_CFLAGS=-I$(SNDFILE_DIR)/src -I$(SNDFILE_BUILDDIR)/src
LOCAL_LIBADD=$(SNDFILELA)

include $(BASE)/build/modmake.rules

$(SNDFILELA): $(SNDFILE_DIR) $(SNDFILE_DIR)/.update
	cd $(SNDFILE_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
