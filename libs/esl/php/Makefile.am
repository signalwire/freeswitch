WRAP_GCC_WARNING_SILENCE=-Wno-unused-label -Wno-unused-function

all: ESL.so

esl_wrap.cpp:
	swig2.0 -module ESL -php5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i
	sed -e 's/  char \*type_name;/  const char \*type_name;/' -i esl_wrap.cpp

esl_wrap.o: esl_wrap.cpp
	$(CXX) -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS) $(CXXFLAGS) $(PHP_CFLAGS) $(WRAP_GCC_WARNING_SILENCE) -c esl_wrap.cpp -o esl_wrap.o

ESL.so: esl_wrap.o
	$(CXX) $(SOLINK) esl_wrap.o $(MYLIB) $(PHP_LDFLAGS) $(PHP_LIBS) -lpthread -o ESL.so -L. $(LIBS)

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
