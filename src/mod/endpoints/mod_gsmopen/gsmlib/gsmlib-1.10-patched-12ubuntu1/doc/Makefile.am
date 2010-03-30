## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: doc Makefile
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 11.6.1999
# *************************************************************************

man_MANS =	gsmsmsd.8 gsmctl.1 gsmpb.1 gsmsendsms.1 gsmsmsstore.1 gsminfo.7

EXTRA_DIST =	gsmsmsd.man gsmctl.man gsmpb.man gsmsendsms.man \
		gsmsmsstore.man gsmlib.lsm gsminfo.man \
		README.NLS README.developers FAQ

%.1: %.man
		sed -e "s/##VERSION##/$(VERSION)/g;s/##DATE##/`date`/g" \
		$< > $@

%.7: %.man
		sed -e "s/##VERSION##/$(VERSION)/g;s/##DATE##/`date`/g" \
		$< > $@

%.8: %.man
		sed -e "s/##VERSION##/$(VERSION)/g;s/##DATE##/`date`/g" \
		$< > $@

clean-local:
		rm -f *.1 *.7 *.8
