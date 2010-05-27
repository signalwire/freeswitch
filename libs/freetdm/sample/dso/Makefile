CC=gcc
CFLAGS=-Wall -I/usr/local/freeswitch/include 
LDFLAGS=-L/usr/local/freeswitch/lib -lfreetdm

ftdmload: ftdmload.o

clean:
	rm -rf ftdmload.o
	rm -rf ftdmload

export:
	export LD_LIBRARY_PATH=/usr/local/freeswitch/lib
	

