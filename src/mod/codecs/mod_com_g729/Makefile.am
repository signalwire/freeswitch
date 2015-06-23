include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_com_g729
VERSION=201506221434

if ISLINUX

G729INSTALLER = $(top_srcdir)/libs/fs-$(VERSION)-installer
LICSERVER=/usr/sbin/freeswitch_licence_server
VALIDATOR=$(bindir)/validator
MOD=$(moddir)/mod_com_g729.so
BUILT_SOURCES = $(G729INSTALLER)

install: $(LICSERVER) $(VALIDATOR) $(MOD)

$(LICSERVER) $(VALIDATOR) $(MOD): $(G729INSTALLER)
	$(SHELL) $(G729INSTALLER) $(bindir) $(moddir) nobanner
	@echo
	@echo
	@echo Now you can activate your license by running $(MAKE) mod_com_g729-activate
	@echo
	@echo

$(G729INSTALLER):
	rm -f $(top_srcdir)/libs/fs-*-installer*
	$(GETG729) fs-$(VERSION)-installer
	chmod 755 $(G729INSTALLER)

clean:
	rm -f $(top_srcdir)/libs/fs-*-installer*
	rm -rf /tmp/fsg729

activate: $(LICSERVER) $(VALIDATOR) $(MOD)
	$(VALIDATOR)

uninstall: clean
	rm -f $(LICSERVER) $(VALIDATOR) $(MOD)

endif
