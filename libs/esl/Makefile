PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
LIBEDIT_DIR=../../libs/libedit
CFLAGS=$(INCS) -g -ggdb -I$(LIBEDIT_DIR)/src/
MYLIB=libesl.a
LIBS=-lesl -lncurses -lpthread
LDFLAGS=-L.
OBJS=src/esl.o src/esl_event.o src/esl_threadmutex.o src/esl_config.o

all: $(MYLIB) fs_cli testclient testserver

$(MYLIB): $(OBJS) $(HEADERS)
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

clean:
	rm -f *.o src/*.o testclient testserver fs_cli libesl.a *~ src/*~ src/include/*~

