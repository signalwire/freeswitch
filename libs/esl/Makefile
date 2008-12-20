PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
LIBEDIT_DIR=../../libs/libedit
CFLAGS=$(INCS) -g -ggdb -I$(LIBEDIT_DIR)/src/
MYLIB=libesl.a

OBJS=src/esl.o src/esl_event.o src/esl_threadmutex.o src/esl_config.o

all: $(MYLIB) fs_cli

$(MYLIB): $(OBJS) $(HEADERS)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

%.o: %.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

test: $(MYLIB)
	$(CC) $(CC_CFLAGS) $(CFLAGS) testclient.c -o testclient -L. -lesl

fs_cli: $(MYLIB) fs_cli.c
	$(CC) $(CC_CFLAGS) $(CFLAGS) fs_cli.c -o fs_cli -L. -L$(LIBEDIT_DIR)/src/.libs -lesl -ledit -lncurses -lpthread

clean:
	rm -f *.o src/*.o testclient fs_cli libesl.a *~ src/*~ src/include/*~

