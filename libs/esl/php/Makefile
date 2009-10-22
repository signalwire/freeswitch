LOCAL_CFLAGS=$(shell php-config --includes)
PHP_LDFLAGS=$(shell php-config --ldflags)
PHP_LIBS=$(shell php-config --libs | sed -r 's/ ?-l(bz2|pcre|xml2|gssapi_krb5|krb5|k5crypto|com_err|history|z|readline|gmp|ssl|crypto)//g')
LOCAL_LDFLAGS=$(PHP_LDFLAGS) $(PHP_LIBS)
LOCAL_LDFLAGS+=-lpthread
WRAP_GCC_WARNING_SILENCE=-Wno-unused-label -Wno-unused-function

all: ESL.so

esl_wrap.cpp:
	swig -module ESL -php5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

esl_wrap.o: esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) $(WRAP_GCC_WARNING_SILENCE) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(LOCAL_LDFLAGS) -o ESL.so -L. $(LIBS)

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so php_ESL.* ESL.php

reswig:	swigclean esl_wrap.cpp

install: ESL.so
	cp ESL.so $(shell php-config --extension-dir)
	cp ESL.php $(shell php -r 'echo ini_get("include_path");' | cut -d: -f2)
	echo 'extension=ESL.so' > $(shell php-config --configure-options | tr " " "\n" | grep -- --with-config-file-scan-dir | cut -f2 -d=)/esl.ini
