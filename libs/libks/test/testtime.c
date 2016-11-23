#include "ks.h"
#include "tap.h"

int main(int argc, char **argv)
{
	int64_t now, then;
	int diff;
	int i;

	ks_init();

	plan(2);

	then = ks_time_now();
	
	ks_sleep(2000000);

	now = ks_time_now();

	diff = (int)((now - then) / 1000);
	printf("DIFF %ums\n", diff);
	

	ok( diff > 1990 && diff < 2010 );

	then = ks_time_now();

	for (i = 0; i < 100; i++) {
		ks_sleep(20000);
	}

	now = ks_time_now();

	diff = (int)((now - then) / 1000);
	printf("DIFF %ums\n", diff);

#if defined(__APPLE__)
	/* the clock on osx seems to be particularly bad at being accurate, we need a bit more room for error*/
	ok( diff > 1900 && diff < 2100 );
#else
	ok( diff > 1950 && diff < 2050 );
#endif
	ks_shutdown();
	done_testing();
}
