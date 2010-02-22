MAINTAINERCLEANFILES   = Makefile.in

def-data:
	test -d $(datadir) || $(mkinstalldirs) $(datadir)
	for datafile in `find ./ -name \*.pcm -o -name \*.xml -o -name \*.jsgf -o -name \*.txt` ; do \
	    filename=`echo $$datafile | sed -e 's|^.*/||'`; \
	    $(INSTALL) -m 644 $$filename $(datadir); \
	done
           
install-data-local:
	test -d $(datadir) || $(MAKE) def-data
