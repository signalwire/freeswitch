MAINTAINERCLEANFILES   = Makefile.in

def-conf:
	test -d $(confdir) || $(mkinstalldirs) $(confdir)
	for conffile in `find ./ -name \*.xml` ; do \
	    filename=`echo $$conffile | sed -e 's|^.*/||'`; \
	    $(INSTALL) -m 644 $$filename $(confdir); \
	done

install-data-local:
	test -d $(confdir) || $(MAKE) def-conf
