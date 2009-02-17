PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
LIBEDIT_DIR=../../libs/libedit
DEBUG=-g -ggdb
BASE_FLAGS=$(INCS) -DHAVE_EDITLINE $(DEBUG) -I$(LIBEDIT_DIR)/src/ -fPIC
PICKY=-O2 -ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes
CFLAGS=$(BASE_FLAGS) $(PICKY)
CXXFLAGS=$(BASE_FLAGS)
MYLIB=libesl.a
LIBS=-lncurses -lpthread -lesl
LDFLAGS=-L.
OBJS=src/esl.o src/esl_event.o src/esl_threadmutex.o src/esl_config.o
SRC=src/esl.c src/esl_event.c src/esl_threadmutex.c src/esl_config.c src/esl_oop.cpp
HEADERS=src/include/esl_config.h src/include/esl_event.h src/include/esl.h src/include/esl_threadmutex.h src/include/esl_oop.h
SOLINK=-shared -Xlinker -x
# comment the next line to disable c++ (no swig mods for you then)
OBJS += src/esl_oop.o

all: $(MYLIB) fs_cli testclient testserver

$(MYLIB): $(OBJS) $(HEADERS) $(SRC)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

testserver: $(MYLIB) testserver.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testserver.c -o testserver $(LDFLAGS) $(LIBS)

testclient: $(MYLIB) testclient.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testclient.c -o testclient $(LDFLAGS) $(LIBS)

fs_cli: $(MYLIB) fs_cli.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) fs_cli.c -o fs_cli $(LDFLAGS) -L$(LIBEDIT_DIR)/src/.libs $(LIBS) -ledit

%.o: %.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o src/*.o testclient testserver fs_cli libesl.a *~ src/*~ src/include/*~
	make -C perl clean
	make -C php clean
	make -C lua clean
	make -C python clean
	make -C ruby clean

reswig:	swigclean
	make -C perl reswig
	make -C php reswig
	make -C lua reswig
	make -C python reswig
	make -C ruby reswig

swigclean: clean
	make -C perl swigclean
	make -C php swigclean
	make -C lua swigclean
	make -C python swigclean
	make -C ruby swigclean

perlmod: $(MYLIB)
	make MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C perl

phpmod: $(MYLIB)
	make MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C php

luamod: $(MYLIB)
	make MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C lua

pymod: $(MYLIB)
	make MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C python

rubymod: $(MYLIB)
	make MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C ruby


everymod: perlmod phpmod luamod pymod rubymod
