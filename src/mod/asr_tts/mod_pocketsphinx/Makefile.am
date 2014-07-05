include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_pocketsphinx

SPHINXBASE=sphinxbase-0.8
SPHINXBASE_DIR=$(switch_srcdir)/libs/$(SPHINXBASE)
SPHINXBASE_BUILDDIR=$(switch_builddir)/libs/$(SPHINXBASE)
POCKETSPHINX=pocketsphinx-0.8
POCKETSPHINX_DIR=$(switch_srcdir)/libs/$(POCKETSPHINX)
POCKETSPHINX_BUILDDIR=$(switch_builddir)/libs/$(POCKETSPHINX)
SPHINXBASE_LA=$(SPHINXBASE_BUILDDIR)/src/libsphinxbase/libsphinxbase.la
SPHINXBASE_LA2=$(SPHINXBASE_BUILDDIR)/src/libsphinxad/libsphinxad.la

POCKETSPHINX_LA=$(POCKETSPHINX_BUILDDIR)/src/libpocketsphinx/libpocketsphinx.la

SPHINXMODEL=communicator_semi_6000_20080321

mod_LTLIBRARIES = mod_pocketsphinx.la
mod_pocketsphinx_la_SOURCES  = mod_pocketsphinx.c
mod_pocketsphinx_la_CFLAGS   = $(AM_CFLAGS) -I$(SPHINXBASE_DIR)/include -I$(POCKETSPHINX_DIR)/include
mod_pocketsphinx_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(POCKETSPHINX_LA) $(SPHINXBASE_LA) $(SPHINXBASE_LA2)
mod_pocketsphinx_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(POCKETSPHINX_LA) $(SPHINXBASE_LA) $(SPHINXBASE_LA2)

$(SPHINXBASE_DIR):
	$(GETLIB) $(SPHINXBASE).tar.gz

$(SPHINXBASE_BUILDDIR)/Makefile: $(SPHINXBASE_DIR)
	mkdir -p $(SPHINXBASE_BUILDDIR)
	(cd $(SPHINXBASE_BUILDDIR) && $(DEFAULT_VARS) $(SPHINXBASE_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(SPHINXBASE_DIR) --without-python CFLAGS=)
	$(TOUCH_TARGET)

$(SPHINXBASE_BUILDDIR)/buildstamp: $(SPHINXBASE_BUILDDIR)/Makefile
	(cd $(SPHINXBASE_BUILDDIR) && $(MAKE))
	touch $@

$(SPHINXBASE_LA): $(SPHINXBASE_BUILDDIR)/buildstamp
	$(TOUCH_TARGET)

$(SPHINXBASE_LA2): $(SPHINXBASE_BUILDDIR)/buildstamp
	$(TOUCH_TARGET)

$(POCKETSPHINX_DIR):
	$(GETLIB) $(POCKETSPHINX).tar.gz

$(POCKETSPHINX_BUILDDIR)/Makefile: $(POCKETSPHINX_DIR) $(SPHINXBASE_LA) $(SPHINXBASE_LA2)
	mkdir -p $(POCKETSPHINX_BUILDDIR)
	(cd $(POCKETSPHINX_BUILDDIR) && $(DEFAULT_VARS) $(POCKETSPHINX_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(POCKETSPHINX_DIR) --without-python --with-sphinxbase=$(SPHINXBASE_DIR) --with-sphinxbase-build=$(SPHINXBASE_BUILDDIR) CFLAGS=-Wno-return-type)
	$(TOUCH_TARGET)

$(POCKETSPHINX_BUILDDIR)/buildstamp: $(POCKETSPHINX_BUILDDIR)/Makefile
	(cd $(POCKETSPHINX_BUILDDIR) && $(MAKE))
	touch $@

$(POCKETSPHINX_LA): $(POCKETSPHINX_BUILDDIR)/buildstamp
	$(TOUCH_TARGET)

$(DESTDIR)$(grammardir)/model:
	mkdir -p $(DESTDIR)$(grammardir)/model

$(DESTDIR)$(grammardir)/model/communicator:
	$(GETLIB) $(SPHINXMODEL).tar.gz
	mkdir -p $(DESTDIR)$(grammardir)/model/communicator
	cp -rp $(switch_srcdir)/libs/Communicator_semi_40.cd_semi_6000/* $(DESTDIR)$(grammardir)/model/communicator

dictionary:
	@install -m0644 $(POCKETSPHINX_DIR)/model/lm/en_US/cmu07a.dic $(DESTDIR)$(grammardir)/default.dic

install-data-local: $(DESTDIR)$(grammardir)/model $(DESTDIR)$(grammardir)/model/communicator dictionary

