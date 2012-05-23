PERL=$(shell which perl)
PERL_SITEDIR=$(shell perl -MConfig -e 'print $$Config{sitelibexp}')
PERL_LIBDIR=-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS=$(shell perl -MConfig -e 'print $$Config{libs}')
LOCAL_CFLAGS=-w -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts) -DEMBED_PERL
LOCAL_LDFLAGS=$(shell $(PERL) -MExtUtils::Embed -e ldopts) $(shell $(PERL) -MConfig -e 'print $$Config{libs}')
GCC_WARNING_JUNK=-w
PERL_INC=$(shell $(PERL) -MExtUtils::Embed -e perl_inc)
all: FSSCGI.so

scgi_wrap.cpp:
	swig -module FSSCGI -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o scgi_wrap.cpp ../FSSCGI.i

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perlxsi.o: perlxsi.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) $(LOCAL_CFLAGS) -c perlxsi.c -o perlxsi.o

scgi_wrap.o: scgi_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(GCC_WARNING_JUNK) $(PERL_INC) -c scgi_wrap.cpp -o scgi_wrap.o

FSSCGI.so: scgi_wrap.o perlxsi.o
	$(CXX) $(SOLINK) scgi_wrap.o perlxsi.o $(MYLIB) $(LOCAL_LDFLAGS) -o FSSCGI.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f scgi_wrap.* FSSCGI.so FSSCGI.pm perlxsi.*

reswig:	swigclean scgi_wrap.cpp perlxsi.c

install: FSSCGI.so
	install -m 755 FSSCGI.so $(PERL_SITEDIR)
	install -m 755 FSSCGI.pm $(PERL_SITEDIR)
	install -d -m 755 FSSCGI $(PERL_SITEDIR)/FSSCGI
	install -m 755 FSSCGI/* $(PERL_SITEDIR)/FSSCGI
