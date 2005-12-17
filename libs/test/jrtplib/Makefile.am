SUBDIRS = src tools doc examples pkgconfig
EXTRA_DIST = LICENSE.MIT ChangeLog README.TXT jrtplib.dsw jrtplib.dsp jrtplib.vcproj \
	     jrtplib.sln jrtplib_wce.vcproj jrtplib_wce.sln

dist-hook: distclean
	rm -f $(distdir)/src/rtpconfig_unix.h
	rm -f $(distdir)/src/rtptypes_unix.h
	mv $(distdir)/jrtplib.dsw $(distdir)/jrtplib.dsw.tmp
	mv $(distdir)/jrtplib.dsp $(distdir)/jrtplib.dsp.tmp
	todos < $(distdir)/jrtplib.dsw.tmp > $(distdir)/jrtplib.dsw
	todos < $(distdir)/jrtplib.dsp.tmp > $(distdir)/jrtplib.dsp
	rm $(distdir)/jrtplib.dsw.tmp
	rm $(distdir)/jrtplib.dsp.tmp
	chmod a-x $(distdir)/jrtplib.vcproj
	chmod a-x $(distdir)/jrtplib.sln
	chmod a-x $(distdir)/jrtplib_wce.vcproj
	chmod a-x $(distdir)/jrtplib_wce.sln
