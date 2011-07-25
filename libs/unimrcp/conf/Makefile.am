MAINTAINERCLEANFILES   = Makefile.in

def-conf:
	test -d $(confdir) || $(mkinstalldirs) $(confdir)
	for conffile in `find ./ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
	    filename=`echo $$conffile | sed -e 's|^.*/||'`; \
	    $(INSTALL) -m 644 $$filename $(confdir); \
	done

	test -d $(confdir)/client-profiles || $(mkinstalldirs) $(confdir)/client-profiles
	for conffile in `find ./client-profiles/ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
	    filename=`echo $$conffile | sed -e 's|^.*/||'`; \
	    $(INSTALL) -m 644 client-profiles/$$filename $(confdir)/client-profiles; \
	done


install-data-local:
	test -d $(confdir) || $(MAKE) def-conf
