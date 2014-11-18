MAINTAINERCLEANFILES = Makefile.in

CONFFILES            = logger.xml dirlayout.xml

if UNIMRCP_CLIENT_LIB
CONFFILES           += unimrcpclient.xml unimrcpclient.xsd
endif

if UNIMRCP_SERVER_LIB
CONFFILES           += unimrcpserver.xml unimrcpserver.xsd
endif

if UMC
CONFFILES           += umcscenarios.xml
endif

def-conf:
	test -d $(DESTDIR)$(sysconfdir) || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	for conffile in $(CONFFILES) ; do \
		$(INSTALL) -m 644 $(top_srcdir)/conf/$$conffile $(DESTDIR)$(sysconfdir); \
	done
if UNIMRCP_CLIENT_LIB
	test -d $(DESTDIR)$(sysconfdir)/client-profiles || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/client-profiles
	for conffile in `find $(top_srcdir)/conf/client-profiles/ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 $(top_srcdir)/conf/client-profiles/$$filename $(DESTDIR)$(sysconfdir)/client-profiles; \
	done
endif

install-data-local:
	test -d $(DESTDIR)$(sysconfdir) || $(MAKE) def-conf
