PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
LIBEDIT_DIR=../../libs/libedit
DEBUG=-g -ggdb
BASE_FLAGS=$(INCS) -DHAVE_EDITLINE $(DEBUG) -I$(LIBEDIT_DIR)/src/ -fPIC
PICKY=-O2 -ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes
CFLAGS=$(BASE_FLAGS) $(PICKY)
CXXFLAGS=$(BASE_FLAGS) -Wall -Werror -Wno-unused-variable
MYLIB=libesl.a
LIBS=-lncurses -lpthread -lesl
LDFLAGS=-L.
OBJS=src/esl.o src/esl_event.o src/esl_threadmutex.o src/esl_config.o
SRC=src/esl.c src/esl_event.c src/esl_threadmutex.c src/esl_config.c src/esl_oop.cpp
HEADERS=src/include/esl_config.h src/include/esl_event.h src/include/esl.h src/include/esl_threadmutex.h src/include/esl_oop.h
SOLINK=-shared -Xlinker -x
# comment the next line to disable c++ (no swig mods for you then)
OBJS += src/esl_oop.o

all: $(MYLIB) fs_cli testclient testserver ivrd

$(MYLIB): $(OBJS) $(HEADERS) $(SRC)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

testserver: $(MYLIB) testserver.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testserver.c -o testserver $(LDFLAGS) $(LIBS)

ivrd: $(MYLIB) ivrd.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) ivrd.c -o ivrd $(LDFLAGS) $(LIBS)

testclient: $(MYLIB) testclient.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testclient.c -o testclient $(LDFLAGS) $(LIBS)

fs_cli: $(MYLIB) fs_cli.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) fs_cli.c -o fs_cli $(LDFLAGS) -L$(LIBEDIT_DIR)/src/.libs $(LIBS) -ledit

%.o: %.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o src/*.o testclient testserver ivrd fs_cli libesl.a *~ src/*~ src/include/*~
	$(MAKE) -C perl clean
	$(MAKE) -C php clean
	$(MAKE) -C lua clean
	$(MAKE) -C python clean
	$(MAKE) -C ruby clean
	$(MAKE) -C java clean
	$(MAKE) -C managed clean

reswig:	swigclean
	$(MAKE) -C perl reswig
	$(MAKE) -C php reswig
	$(MAKE) -C lua reswig
	$(MAKE) -C python reswig
	$(MAKE) -C ruby reswig
	$(MAKE) -C java reswig
	$(MAKE) -C managed reswig

swigclean: clean
	$(MAKE) -C perl swigclean
	$(MAKE) -C php swigclean
	$(MAKE) -C lua swigclean
	$(MAKE) -C python swigclean
	$(MAKE) -C ruby swigclean
	$(MAKE) -C java swigclean
	$(MAKE) -C managed swigclean

perlmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C perl

phpmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C php 

luamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C lua

pymod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C python

tclmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C tcl

rubymod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C ruby

javamod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C java

managedmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C managed

phpmod-install: phpmod
	$(MAKE) -C php install

everymod: perlmod phpmod luamod pymod rubymod javamod managedmod
