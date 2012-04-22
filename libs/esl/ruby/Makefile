## no idea how to simply ask ruby which one to use
LOCAL_CFLAGS=-I$(shell ruby -e 'require "rbconfig"; puts RbConfig::CONFIG["topdir"]')
LOCAL_LDFLAGS=$(shell ruby -e 'require "rbconfig"; puts RbConfig::CONFIG["LIBS"]')

all: ESL.so

esl_wrap.cpp:
	swig -module ESL -ruby -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so

reswig:	swigclean esl_wrap.cpp

