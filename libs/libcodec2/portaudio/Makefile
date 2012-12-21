# Makefile for Portaudio test programs
# Requires Portaudio V19

CFLAGS =  -g -Wall -I../src
LIBS = -lm -lportaudio -pthread
SRC  = ../src/fdmdv.c ../src/kiss_fft.c ../src/fifo.c

all: pa_rec pa_play pa_recplay pa_impresp

pa_rec: Makefile pa_rec.c $(SRC)
	gcc $(CFLAGS) pa_rec.c $(SRC) -o pa_rec $(LIBS)

pa_play: Makefile pa_play.c $(SRC)
	gcc $(CFLAGS) pa_play.c $(SRC) -o pa_play $(LIBS)

pa_recplay: Makefile pa_recplay.c $(SRC)
	gcc $(CFLAGS) pa_recplay.c $(SRC) -o pa_recplay $(LIBS)

pa_impresp: Makefile pa_impresp.c $(SRC)
	gcc $(CFLAGS) pa_impresp.c $(SRC) -o pa_impresp $(LIBS)

clean:
	rm -f pa_rec pa_play pa_recplay
