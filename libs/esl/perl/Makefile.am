all: ESL.so

esl_wrap.cpp:
	swig -module ESL -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perlxsi.o: perlxsi.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS) $(PERL_CFLAGS) -c perlxsi.c -o perlxsi.o

esl_wrap.o: esl_wrap.cpp
	$(CXX) -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS) $(CXXFLAGS) -w $(PERL_INC) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o perlxsi.o
	$(CXX) $(SOLINK) esl_wrap.o perlxsi.o ../.libs/libesl.a $(PERL__LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so ESL.pm perlxsi.*

reswig:	swigclean esl_wrap.cpp perlxsi.c

install: ESL.so
	install -m 755 ESL.so $(PERL_SITEDIR)
	install -m 755 ESL.pm $(PERL_SITEDIR)
	install -d -m 755 ESL $(PERL_SITEDIR)/ESL
	install -m 755 ESL/* $(PERL_SITEDIR)/ESL
