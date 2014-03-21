all: ESL.so

esl_wrap.cpp:
	swig -module ESL -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so ESL.pm perlxsi.*

reswig:	swigclean esl_wrap.cpp perlxsi.c

