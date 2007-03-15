MODNAME=mod_spidermonkey_odbc
switch_srcdir=../../../..

ODBC=unixODBC-2.2.12
ODBC_DIR=$(switch_srcdir)/libs/$(ODBC)
ODBCLA=libodbc.la
UNINST_ODBCLA=$(ODBC_DIR)/DriverManager/$(ODBCLA)
INST_ODBCLA=$(DESTDIR)$(PREFIX)/lib/$(ODBCLA)
LOCAL_CFLAGS=-I$(ODBC_DIR)/include
LOCAL_LDFLAGS=
LOCAL_LIBADD=$(UNINST_ODBCLA)

include ../mod_spidermonkey/sm.mak

$(ODBC_DIR)/Makefile:
	$(GETLIB) $(ODBC).tar.gz
	cd $(ODBC_DIR) && ./configure --prefix=$(PREFIX) --disable-gui --without-x --with-pic 

$(UNINST_ODBCLA): $(ODBC_DIR)/Makefile
	cd $(ODBC_DIR) && $(MAKE)

depend_install: $(INST_ODBCLA)

$(INST_ODBCLA): $(UNINST_ODBCLA)
	cd $(ODBC_DIR) && $(MAKE) install

