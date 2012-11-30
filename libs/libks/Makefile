PWD=$(shell pwd)
INCS=-I$(PWD)/src/include
DEBUG=-g -ggdb
BASE_FLAGS=$(INCS) $(DEBUG) -I$(LIBEDIT_DIR)/src/ -fPIC
PICKY=-O2
CFLAGS=$(BASE_FLAGS) $(PICKY)
CXXFLAGS=$(BASE_FLAGS)
MYLIB=libks.a
LIBS=-lncurses -lks -lpthread -lm
LDFLAGS=-L.
OBJS=src/ks.o src/ks_threadmutex.o src/ks_config.o src/ks_json.o src/ks_buffer.o src/mpool.o src/table.o src/table_util.o src/simclist.o
SRC=src/ks.c src/ks_json.c src/ks_threadmutex.c src/ks_config.c src/ks_json.c src/ks_buffer.c src/mpool.c src/table.c src/table_util.c src/simclist.c
HEADERS=src/include/ks_config.h src/include/ks.h src/include/ks_threadmutex.h src/include/ks_json.h src/include/ks_buffer.h src/include/mpool.h src/include/mpool_loc.h src/include/table.h src/include/table_loc.h src/include/simclist.h
SOLINK=-shared -Xlinker -x

all: $(MYLIB) 

$(MYLIB): $(OBJS) $(HEADERS) $(SRC)
	ar rcs $(MYLIB) $(OBJS)
	ranlib $(MYLIB)

%.o: %.c $(HEADERS)
	$(CC) $(CC_CFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o src/*.o libks.a *~ src/*~ src/include/*~
