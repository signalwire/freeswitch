INCS=-Isrc/include
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
PERL        = $(shell which perl)
PERL_LIBDIR =-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS   =$(shell perl -MConfig -e 'print $$Config{libs}')
LOCAL_CFLAGS= -w -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts) -DEMBED_PERL
LOCAL_LDFLAGS=$(shell $(PERL) -MExtUtils::Embed -e ldopts) $(shell $(PERL) -MConfig -e 'print $$Config{libs}')
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
	rm -f *.o src/*.o testclient testserver fs_cli libesl.a *~ src/*~ src/include/*~ perl/*.o perl/*.so

reswig:	swigclean perl/esl_wrap.cpp perl/perlxsi.c

swigclean: clean
	rm -f perl/esl_wrap.* perl/ESL.so perl/ESL.pm perl/perlxsi.*

perl/esl_wrap.cpp:
	swig -module ESL -shadow -perl5 -c++ -DMULTIPLICITY -Isrc/include -o perl/esl_wrap.cpp ESL.i

perl/perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perl/perlxsi.c

perl/perlxsi.o: perl/perlxsi.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) $(LOCAL_CFLAGS) -c perl/perlxsi.c -o perl/perlxsi.o

perl/esl_wrap.o: perl/esl_wrap.cpp
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) $(LOCAL_CFLAGS) -c perl/esl_wrap.cpp -o perl/esl_wrap.o

perl/ESL.so: $(MYLIB) perl/esl_wrap.o perl/perlxsi.o
	$(CXX) $(SOLINK) perl/esl_wrap.o perl/perlxsi.o $(LOCAL_LDFLAGS) -o perl/ESL.so -L. $(LIBS)

perlmod: perl/ESL.so



