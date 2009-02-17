LOCAL_CFLAGS=$(shell php-config --includes)
LOCAL_LDFLAGS=$(shell php-config --ldflags --libs)

all: ESL.so

esl_wrap.cpp:
	swig -module ESL -php5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so php_ESL.* ESL.php

reswig:	swigclean esl_wrap.cpp

install: ESL.so
	cp ESL.so $(shell php-config --extension-dir)
	cp ESL.php $(shell php -r 'phpinfo();' | grep include_path | cut -d: -f2)
