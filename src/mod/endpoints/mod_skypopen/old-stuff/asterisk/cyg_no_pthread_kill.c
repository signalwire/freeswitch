#include <stdio.h>
#define PRINTMSGCYG

extern int option_debug;
int cyg_no_pthreadkill(int thread, int sig);

int cyg_no_pthreadkill(int thread, int sig)
{
#ifdef PRINTMSGCYG
	if (option_debug) {
		printf
			("\n\nHere there would have been a pthread_kill() on thread [%-7lx], with sig=%d, but it has been substituted by this printf in file cyg_no_pthread_kill.c because CYGWIN does not support sending a signal to a one only thread :-(\n\n",
			 (unsigned long int) thread, sig);
	}
#endif // PRINTMSGCYG
	return 0;
}
