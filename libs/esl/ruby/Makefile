RUBY:=ruby
LOCAL_LDFLAGS=$(shell $(RUBY) -rrbconfig -e 'puts RbConfig::CONFIG["LIBRUBYARG"]')
LOCAL_CFLAGS=$(shell $(RUBY) cflags.rb)
RUBY_GEM_DIR=$(shell $(RUBY) -e 'puts Gem.dir')

all: ESL.so

esl_wrap.cpp:
	swig2.0 -module ESL -ruby -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)


install: ESL.so
	install -m 755 ESL.so $(RUBY_GEM_DIR)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so

reswig:	swigclean esl_wrap.cpp
