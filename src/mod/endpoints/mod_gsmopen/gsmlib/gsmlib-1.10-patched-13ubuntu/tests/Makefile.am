## Process this file with automake to produce Makefile.in
# *************************************************************************
# * GSM TA/ME library
# *
# * File:    Makefile.am
# *
# * Purpose: tests Makefile
# *
# * Author:  Peter Hofmann (software@pxh.de)
# *
# * Created: 21.5.1999
# *************************************************************************

INCLUDES =		-I..

noinst_PROGRAMS =	testsms testsms2 testparser testgsmlib testpb testpb2 \
			testspb testssms testcb

TESTS =			runspb.sh runspb2.sh runssms.sh runsms.sh \
			runparser.sh runspbi.sh

# test files used for file-based phonebook and SMS testing
EXTRA_DIST =		spb.pb runspb.sh runspb2.sh runssms.sh runsms.sh \
			runparser.sh \
			spb.pb spb2.pb \
			testparser-output.txt testspb-output.txt \
			testssms-output.txt testsms-output.txt \
			testspb2-output.txt \
			runspbi.sh spbi2-orig.pb spbi1.pb testspbi-output.txt

# build testsms from testsms.cc and libgsmme.la
testsms_SOURCES =	testsms.cc
testsms_LDADD =		../gsmlib/libgsmme.la $(INTLLIBS)

# build testsms2 from testsms2.cc and libgsmme.la
testsms2_SOURCES =	testsms2.cc
testsms2_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testparser from testparser.cc and libgsmme.la
testparser_SOURCES =	testparser.cc
testparser_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testgsmlib from testgsmlib.cc and libgsmme.la
testgsmlib_SOURCES =	testgsmlib.cc
testgsmlib_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testpb from testpb.cc and libgsmme.la
testpb_SOURCES =	testpb.cc
testpb_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testpb2 from testpb2.cc and libgsmme.la
testpb2_SOURCES =	testpb2.cc
testpb2_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testspb from testspb.cc and libgsmme.la
testspb_SOURCES =	testspb.cc
testspb_LDADD =	../gsmlib/libgsmme.la $(INTLLIBS)

# build testssms from testssms.cc and libgsmme.la
testssms_SOURCES = testssms.cc
testssms_LDADD = ../gsmlib/libgsmme.la $(INTLLIBS)

# build testcb from testcb.cc and libgsmme.la
testcb_SOURCES = testcb.cc
testcb_LDADD = ../gsmlib/libgsmme.la $(INTLLIBS)
