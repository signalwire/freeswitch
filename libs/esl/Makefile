INCS=-Isrc/include
LIBEDIT_DIR=../../libs/libedit
DEBUG=-g -ggdb
PICKY=-O2 -ffast-math -Wall -Werror -Wunused-variable -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes
CFLAGS=$(INCS) $(DEBUG) -I$(LIBEDIT_DIR)/src/ $(PICKY)
MYLIB=libesl.a
LIBS=-lncurses -lpthread -lesl
LDFLAGS=-L.
OBJS=src/esl.o src/esl_event.o src/esl_threadmutex.o src/esl_config.o
SRC=src/esl.c src/esl_event.c src/esl_threadmutex.c src/esl_config.c
HEADERS=src/include/esl_config.h src/include/esl_event.h src/include/esl.h src/include/esl_threadmutex.h

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

clean:
	rm -f *.o src/*.o testclient testserver fs_cli libesl.a *~ src/*~ src/include/*~

