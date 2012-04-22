LOCAL_CFLAGS=
LOCAL_LDFLAGS=-ltcl
WRAP_GCC_WARNING_SILENCE=-Wno-unused-function

all: ESL.so

esl_wrap.cpp:
	swig -module ESL -tcl -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) $(WRAP_GCC_WARNING_SILENCE) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so

reswig:	swigclean esl_wrap.cpp

