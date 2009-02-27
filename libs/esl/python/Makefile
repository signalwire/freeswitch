LOCAL_CFLAGS=-I/usr/include/python2.4
LOCAL_LDFLAGS=-lpython2.4

all: _ESL.so

esl_wrap.cpp:
	swig -module _ESL -classic -python -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c esl_wrap.cpp -o esl_wrap.o

_ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o _ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so

reswig:	swigclean esl_wrap.cpp

