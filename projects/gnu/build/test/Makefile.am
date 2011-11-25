
# Copyright (c) 2006-2009 Philip R. Zimmermann. All rights reserved.
# Contact: http://philzimmermann.com
# 
# Viktor Krikun <v.krikun@soft-industry.com> <v.krikun@gmail.com>
#



TOP_SRCDIR=$(top_srcdir)/../..
INCLUDES = 	-I$(TOP_SRCDIR)/include \
			-I$(TOP_SRCDIR)/include/enterprise \
			-I$(TOP_SRCDIR)/. \
			-I$(TOP_SRCDIR)/test \
			-I$(TOP_SRCDIR)/test/cmockery \
			-I$(TOP_SRCDIR)/third_party/bgaes \
			-I$(TOP_SRCDIR)/third_party/bnlib


#check_PROGRAMS = cache_test libzrtp_test
check_PROGRAMS = cache_test

### ZRTP Cache testing

cache_test_SOURCES = $(TOP_SRCDIR)/test/cmockery/cmockery.c \
					 $(TOP_SRCDIR)/test/cache_test.c
cache_test_LDADD   = ../libzrtp.a  $(TOP_SRCDIR)/third_party/bnlib/libbn.a -lpthread


### ZRTP high-level test-case 

libzrtp_test_SOURCES =	$(TOP_SRCDIR)/test/pc/zrtp_test_core.c \
					  	$(TOP_SRCDIR)/test/pc/zrtp_test_crypto.c \
						$(TOP_SRCDIR)/test/pc/zrtp_test_queue.c \
						$(TOP_SRCDIR)/test/pc/zrtp_test_ui.c

libzrtp_test_LDADD = ../libzrtp.a $(TOP_SRCDIR)/third_party/bnlib/libbn.a -lpthread

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
