# Makefile for tpl built-in test suite
# 
# This Makefile has three useful targets:
#
# all (default): 
#    Build and run all self-tests by compiling tpl into each test. 
#
#    Note: On Cygwin/MinGW, compiling the tpl source directly into the tests is
#    not supported (as they use 'replacement' functions which get included only
#    in libtpl), so on these platforms the 'alt' target is the default.
#
# alt:
#    Build and run all the self-tests by linking them with libtpl, which must
#    have been created beforehand (by running 'configure; make' in the
#    top-level directory); a reminder will be printed if you have not done so.
#
#    Note, libtool will create wrappers around each test to accomodate the 
#    pre-installed state of ../src/libtpl.la. In a real program, you'd link 
#    against an installed libtpl.la, and these wrappers would not be used.
#
# clean:
#    Clean up all the compiled bits.
#
PROGS = test1 test2 test3 test4 test5 test6 test7 test8 \
	test9 test10 test11 test12 test13 test14 test15 test16 \
	test17 test18 test19 test20 test21 test22 test23 test24 \
	test25 test26 test27 test28 test29 test30 test31 test32 \
	test33 test34 test35 test36 test37 test38 test39 test40 \
	test41 test42 test43 test44 test45 test46 test47 test48 \
	test49 test50 test51 test52 test53 test54 test55 test56 \
	test57 test58 test59 test60 test61 test62 test63 test64 \
    test65 test66 test67 test68 test69 test70 test71 test72 \
    test73 test74 test75 test76 test77 test78 test79 test80 \
    test81 test82 test83 test84 test85 test86 test87 test88 \
    test89 test90 test91 test92 test93 test94 test95 test96 \
    test97 test98 test99 test100 test101 test102 test103 test104 \
    test105 test106 test107 test108 test109 test110 test111 test112 \
    test113 test114 test115 test116 test117 test118 test119 test120 \
    test121 test122 test123 test124

TPLSRC = ../src
CFLAGS = -I$(TPLSRC) -g
CFLAGS += -pedantic 
CFLAGS += -Wall 
#CFLAGS += -m32
#CFLAGS += -m64
CFLAGS += -O3
#For testing without C99 feature support
#CFLAGS += -std=c89

# Prefer 64-bit compilation on Mac OS X (not necessary, just faster)
ifneq ($(strip $(shell $(CC) -v 2>&1 |egrep "i[0-9]+-apple-darwin")),)
  CFLAGS += -m64
endif

# detect Cygwin or MinGW
ifneq ($(strip $(shell $(CC) -v 2>&1 |egrep "cygwin|mingw")),)
  TESTS=./do_tests.cygwin
  # divert to the alt target; use tpl as a lib for Cygwin/Mingw 
  TARGET=alt
else
  TESTS=./do_tests
  TARGET=$(PROGS) run_tests
endif

all: $(TARGET) 

tpl.o :	$(TPLSRC)/tpl.c $(TPLSRC)/tpl.h
	$(CC) -c $(CFLAGS) $(TPLSRC)/tpl.c

$(PROGS) : tpl.o 
	$(CC) $(CFLAGS) -o $@ $(@).c tpl.o

run_tests: 
	perl $(TESTS)

# This target can be used to compile the tests as dependent on
# the tpl library (rather than compiling in the tpl source).
alt: mkalttests run_tests

mkalttests:
	@$(MAKE) -f Makefile.alt PROGS="$(PROGS)" 

.PHONY: clean 

clean:	
	rm -f $(PROGS) tpl.o test*.out test*.err test*.exe 
	rm -rf $(PROGS) test*.dSYM
