
# Copyright (c) 2006-2009 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#



INCLUDES = 	-I$(top_srcdir)/include \
			-I$(top_srcdir)/include/enterprise \
			-I$(top_srcdir)/. \
			-I$(top_srcdir)/test \
			-I$(top_srcdir)/test/cmockery \
			-I$(top_srcdir)/third_party/bgaes \
			-I$(top_srcdir)/third_party/bnlib


check_PROGRAMS = cache_test

### ZRTP Cache testing

cache_test_SOURCES = $(top_srcdir)/test/cmockery/cmockery.c \
					 $(top_srcdir)/test/cache_test.c
cache_test_LDADD   = ../libzrtp.a  $(top_srcdir)/third_party/bnlib/libbn.a -lpthread


SUBDIRS = .

check:	
#	@ ./cache_test

# check:
# 	@echo ""
# 	@echo "*========================================================================*"
# 	@echo "*    starting libZRTP tests                                              *"
# 	@echo "*========================================================================*"
# 	@echo ""
# 	@./libzrtp_test
# 
# 	@echo ""
# 	@echo "*========================================================================*"
# 	@echo "* In case you have a test FAILED send the generated log file       *"
# 	@echo "* with your comment to <zrtp_support@zfoneproject.com>.            *" 
# 	@echo "*========================================================================*"
# 	@echo ""
