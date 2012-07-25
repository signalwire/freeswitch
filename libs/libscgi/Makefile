PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
DEBUG=-g -ggdb
BASE_FLAGS=$(INCS) $(DEBUG) -fPIC
PICKY=-O2
CFLAGS=$(BASE_FLAGS) $(PICKY)
CXXFLAGS=$(BASE_FLAGS)
MYLIB=libscgi.a
LIBS=
LDFLAGS=-L.
OBJS=src/scgi.o
SRC=src/scgi.c src/scgi_oop.cpp
HEADERS=src/include/scgi.h src/include/scgi_oop.h
SOLINK=-shared -Xlinker -x

# comment the next line to disable c++ (no swig mods for you then)
OBJS += src/scgi_oop.o

all: $(MYLIB) testclient testserver

$(MYLIB): $(OBJS) $(HEADERS) $(SRC)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

%.o: %.c $(HEADERS)
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXX_CFLAGS) $(CXXFLAGS) -c $< -o $@

testclient: $(MYLIB) testclient.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testclient.c -o testclient -lscgi $(LDFLAGS) $(LIBS)

testserver: $(MYLIB) testserver.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testserver.c -o testserver -lscgi $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o src/*.o libscgi.a *~ src/*~ src/include/*~ testclient testserver
	$(MAKE) -C perl clean

reswig:	swigclean
	$(MAKE) -C perl reswig

swigclean: clean
	$(MAKE) -C perl swigclean

perlmod: $(MYLIB)
	$(MAKE) MYLIB="../$(MYLIB)" SOLINK="$(SOLINK)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CXX_CFLAGS="$(CXX_CFLAGS)" -C perl

perlmod-install: perlmod
	$(MAKE) -C perl install
