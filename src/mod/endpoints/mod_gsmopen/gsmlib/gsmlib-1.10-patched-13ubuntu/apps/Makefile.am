## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: apps Makefile
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 5.6.1999
# *************************************************************************

INCLUDES =		-I..

bin_PROGRAMS =		gsmsmsstore gsmctl gsmsmsd gsmpb gsmsendsms

# build gsmsmsd from gsmsmsd.cc and libgsmme.la
gsmsmsd_SOURCES =	gsmsmsd.cc
gsmsmsd_LDADD =		../gsmlib/libgsmme.la $(INTLLIBS)

# build gsmpb from gsmpb.cc and libgsmme.la
gsmpb_SOURCES =		gsmpb.cc
gsmpb_LDADD =		../gsmlib/libgsmme.la $(INTLLIBS)

# build gsmctl from gsmctl.cc and libgsmme.la
gsmctl_SOURCES =	gsmctl.cc
gsmctl_LDADD =		../gsmlib/libgsmme.la $(INTLLIBS)

# build gsmsendsms from gsmsendsms.cc and libgsmme.la
gsmsendsms_SOURCES =	gsmsendsms.cc
gsmsendsms_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build gsmsmsstore from gsmsmsstore.cc and libgsmme.la
gsmsmsstore_SOURCES =	gsmsmsstore.cc
gsmsmsstore_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)
