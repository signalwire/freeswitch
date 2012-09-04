all:
	gcc ../mod_posix_timer.c main.c switch.c -I. -o timer_test -lpthread -lrt -lm -g -DLOG_LEVEL=-1

clean:
	-rm timer_test

