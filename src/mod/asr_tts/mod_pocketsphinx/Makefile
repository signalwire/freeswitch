SPHINXBASE=sphinxbase-0.4.99
SPHINXBASE_DIR=$(switch_srcdir)/libs/$(SPHINXBASE)
POCKETSPHINX=pocketsphinx-0.5.99
POCKETSPHINX_DIR=$(switch_srcdir)/libs/$(POCKETSPHINX)
SPHINXBASE_LA=$(SPHINXBASE_DIR)/src/libsphinxbase/libsphinxbase.la
SPHINXBASE_LA2=$(SPHINXBASE_DIR)/src/libsphinxad/libsphinxad.la

POCKETSPHINX_LA=$(POCKETSPHINX_DIR)/src/libpocketsphinx/libpocketsphinx.la

SPHINXMODEL=communicator_semi_6000_20080321

LOCAL_CFLAGS=-I$(SPHINXBASE_DIR)/include -I$(POCKETSPHINX_DIR)/include
LOCAL_LIBADD=$(SPHINXBASE_LA) $(SPHINXBASE_LA2) $(POCKETSPHINX_LA) 
BASE=../../../..
include $(BASE)/build/modmake.rules

$(SPHINXBASE_DIR):
	$(GETLIB) $(SPHINXBASE)-latest.tar.gz

$(SPHINXBASE_DIR)/buildstamp: $(SPHINXBASE_DIR)
	test -f $(SPHINXBASE_DIR)/Makefile || (cd $(SPHINXBASE_DIR) && ./configure --with-pic --without-python --disable-shared CFLAGS=)
	test -f $(SPHINXBASE_DIR)/buildstamp || (touch $(SPHINXBASE_DIR)/buildstamp)

$(SPHINXBASE_LA): $(SPHINXBASE_DIR)/buildstamp
	cd $(SPHINXBASE_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(POCKETSPHINX_DIR):
	$(GETLIB) $(POCKETSPHINX)-latest.tar.gz

$(POCKETSPHINX_DIR)/Makefile: $(POCKETSPHINX_DIR)
	cd $(POCKETSPHINX_DIR) && ./configure --with-pic --without-python --disable-shared --with-sphinxbase=$(SPHINXBASE_DIR) CFLAGS=
	$(TOUCH_TARGET)

$(POCKETSPHINX_DIR)/buildstamp: $(POCKETSPHINX_DIR)/Makefile
	test -f $(POCKETSPHINX_DIR)/buildstamp || touch $(POCKETSPHINX_DIR)/buildstamp

$(POCKETSPHINX_LA): $(POCKETSPHINX_DIR)/Makefile 
	cd $(POCKETSPHINX_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(DESTDIR)$(PREFIX)/grammar/model:
	mkdir -p $(DESTDIR)$(PREFIX)/grammar/model

$(DESTDIR)$(PREFIX)/grammar/model/communicator:
	$(GETLIB) $(SPHINXMODEL).tar.gz
	mkdir -p $(DESTDIR)$(PREFIX)/grammar/model/communicator
	cp -drp $(BASE)/libs/Communicator_semi_40.cd_semi_6000/* $(DESTDIR)$(PREFIX)/grammar/model/communicator

$(DESTDIR)$(PREFIX)/grammar/model/wsj1:
	mkdir -p $(DESTDIR)$(PREFIX)/grammar/model/wsj1
	cp -drp $(POCKETSPHINX_DIR)/model/hmm/wsj1/* $(DESTDIR)$(PREFIX)/grammar/model/wsj1

dictionary:
	@cp -f $(POCKETSPHINX_DIR)/model/lm/cmudict.0.6d $(DESTDIR)$(PREFIX)/grammar/default.dic

local_install: $(DESTDIR)$(PREFIX)/grammar/model $(DESTDIR)$(PREFIX)/grammar/model/communicator $(DESTDIR)$(PREFIX)/grammar/model/wsj1 dictionary
