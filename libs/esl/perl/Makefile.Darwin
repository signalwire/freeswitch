PERL=$(shell which perl)
PERL_LIBDIR=-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS=$(shell perl -MConfig -e 'print $$Config{libs}')
LOCAL_CFLAGS=-w -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts) -DEMBED_PERL
LOCAL_LDFLAGS=$(shell $(PERL) -MExtUtils::Embed -e ldopts) $(shell $(PERL) -MConfig -e 'print $$Config{libs}')
GCC_WARNING_JUNK=-w
PERL_INC=$(shell $(PERL) -MExtUtils::Embed -e perl_inc)
all: ESL.so

esl_wrap.cpp:
	swig -module ESL -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perlxsi.o: perlxsi.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) $(LOCAL_CFLAGS) -c perlxsi.c -o perlxsi.o

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(GCC_WARNING_JUNK) $(PERL_INC) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o perlxsi.o
	$(CXX) $(SOLINK) esl_wrap.o perlxsi.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so ESL.pm perlxsi.*

reswig:	swigclean esl_wrap.cpp perlxsi.c

