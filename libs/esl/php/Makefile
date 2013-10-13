PHP=php
PHP_CONFIG=php-config

PHP_LDFLAGS=$(shell $(PHP_CONFIG) --ldflags)
PHP_LIBS=$(shell $(PHP_CONFIG) --libs | sed -r 's/ ?-l(bz2|pcre|xml2|gssapi_krb5|krb5|k5crypto|com_err|history|z|readline|gmp|ssl|crypto)//g')
PHP_EXT_DIR=$(shell $(PHP_CONFIG) --extension-dir)
PHP_INC_DIR=$(shell $(PHP) -r 'echo ini_get("include_path");' | cut -d: -f2)
PHP_INI_DIR=$(shell $(PHP_CONFIG) --configure-options | tr " " "\n" | grep -- --with-config-file-scan-dir | cut -f2 -d=)

LOCAL_CFLAGS=$(shell php-config --includes)
LOCAL_LDFLAGS=$(PHP_LDFLAGS) $(PHP_LIBS)
LOCAL_LDFLAGS+=-lpthread

WRAP_GCC_WARNING_SILENCE=-Wno-unused-label -Wno-unused-function

all: ESL.so

esl_wrap.cpp:
	swig -module ESL -php5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i
	sed -e 's/  char \*type_name;/  const char \*type_name;/' -i esl_wrap.cpp

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
	test -d "$(DESTDIR)$(PHP_EXT_DIR)" || mkdir -p "$(DESTDIR)$(PHP_EXT_DIR)"
	cp ESL.so "$(DESTDIR)$(PHP_EXT_DIR)"
	test -d "$(DESTDIR)$(PHP_INC_DIR)" || mkdir -p "$(DESTDIR)$(PHP_INC_DIR)"
	cp ESL.php "$(DESTDIR)$(PHP_INC_DIR)"
	test -d "$(DESTDIR)$(PHP_INI_DIR)" || mkdir -p "$(DESTDIR)$(PHP_INI_DIR)"
	test -f "$(DESTDIR)$(PHP_INI_DIR)/esl.ini" || echo 'extension=ESL.so' > "$(DESTDIR)$(PHP_INI_DIR)/esl.ini"
