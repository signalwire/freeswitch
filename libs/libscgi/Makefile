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
SRC=src/scgi.c 
HEADERS=src/include/scgi.h 
SOLINK=-shared -Xlinker -x


all: $(MYLIB) testclient

$(MYLIB): $(OBJS) $(HEADERS) $(SRC)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

%.o: %.c $(HEADERS)
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

testclient: $(MYLIB) testclient.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) testclient.c -o testclient -lscgi $(LDFLAGS) $(LIBS)



clean:
	rm -f *.o src/*.o libscgi.a *~ src/*~ src/include/*~ testclient
