
#include <switch.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>

extern SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load);
extern SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown);
extern SWITCH_MODULE_RUNTIME_FUNCTION(mod_posix_timer_runtime);

switch_loadable_module_interface_t *mod = NULL;
switch_memory_pool_t pool = { 0 };
switch_timer_interface_t *timer_if;
pthread_t module_runtime_thread_id;

pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
int pass_count;
int warn_count;
int fail_count;
int total_sessions;
int session_count;
int last_reported_session_count;
int shutdown;


/**
 * Return a random sample from a normal distrubtion centered at mean with 
 * the specified standard deviation.
 *
 * THIS FUNCTION IS NOT REENTRANT!!!
 */
double randnorm(double mean, double std_dev)
{
	static double z1 = -1.0f;
	double u1, u2, z0;

	/* random numbers are generated in pairs.  See if new pair needs to be calculated */
	if (z1 >= 0.0f) {
		z0 = z1;
		z1 = -1.0f;
	} else {
		/* use box-muller transform to generate random number pair over normal distribution */
		u1 = drand48();
		u2 = drand48();
		z0 = sqrt(-2.0f * log(u1)) * cos(2.0f * M_PI * u2);
		z1 = sqrt(-2.0f * log(u1)) * sin(2.0f * M_PI * u2);
	}

	return (z0 * std_dev) + mean;
}

/**
 * Pick a random sample according the the weights
 * @param weights array of weights
 * @param num_weights
 */
static int sample(int *weights, int num_weights)
{
	int total_weight = weights[num_weights - 1];
	int s = floor(drand48() * total_weight);
	int i;
	for (i = 0; i < num_weights; i++) {
		if (s < weights[i]) {
			return i;
		}
	}
	printf ("DOH! s = %f\n", s);
	return 0;
}

/* 
 * Calculate x - y
 * @return 0 if x is before y, the difference otherwise.
 */
double timespec_subtract(struct timespec *x, struct timespec *y)
{
	struct timespec result;

	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_nsec < y->tv_nsec) {
		int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
		y->tv_nsec -= 1000000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_nsec - y->tv_nsec > 1000000000) {
		int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
		y->tv_nsec += 1000000000 * nsec;
		y->tv_sec -= nsec;
	}
     
	/* Return 0 if result is negative. */
	if(x->tv_sec < y->tv_sec) {
		return 0.0f;
	}

	/* Return the difference */
	result.tv_sec = x->tv_sec - y->tv_sec;
	result.tv_nsec = x->tv_nsec - y->tv_nsec;
	return (double)result.tv_sec + (double)(result.tv_nsec / 1e9);
}

/**
 * Entry point for the runtime thread
 */
static void *module_thread(void *dummy)
{
	mod_posix_timer_runtime();
	return NULL;
}

/**
 * Load mod_posix_timer and start the runtime thread
 */
static int load_module()
{
	fail_count = 0;
	warn_count = 0;
	pass_count = 0;
	total_sessions = 0;
	session_count = 0;
	last_reported_session_count = 0;
	shutdown = 0;
	if (mod_posix_timer_load(&mod, &pool) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}
	timer_if = mod->timer;
	return pthread_create(&module_runtime_thread_id, NULL, module_thread, NULL);
}

/**
 * Shutdown mod_posix_timer
 */
static void shutdown_module()
{
	shutdown = 1;
	mod_posix_timer_shutdown();
	pthread_join(module_runtime_thread_id, NULL);
}

/**
 * Test rapidly creating and destroying timers
 */
static void test_create_destroy()
{
	switch_timer_t *timers[3000] = { 0 };
	int intervals[4] = { 10, 20, 30, 40 };
	int interval_weights[4] = { 25, 50, 75, 100 };
	int interval_counts[4] = { 0, 0, 0, 0 };
	int toggle[2] = { 75, 100 };
	int timer_count = 0;
	
	int i = 0;
	printf("test_create_destroy()\n");
	for(i = 0; i < 100000000; i++) {
		int clear = i % 100000 == 0;
		int j;
		for (j = 0; j < 3000; j++) {
			if (sample(toggle, 2) || clear) {
				if (timers[j]) {
					interval_counts[timers[j]->interval / 10 - 1]--;
					timer_if->timer_destroy(timers[j]);
					free(timers[j]);
					timers[j] = NULL;
					timer_count--;
				} else if (!clear) {
					int interval = intervals[sample(interval_weights, 4)];
					timers[j] = malloc(sizeof(switch_timer_t));
					memset(timers[j], 0, sizeof(switch_timer_t));
					timers[j]->interval = interval;
					timers[j]->samples = interval * 8;
					timer_if->timer_init(timers[j]);
					timer_count++;
					interval_counts[interval / 10 - 1]++;
				}
			}
		}
		if (i % 1000 == 0) {
			printf("timers = %d, 10ms = %d, 20ms = %d, 30ms = %d, 40ms = %d\n", timer_count, interval_counts[0], interval_counts[1], interval_counts[2], interval_counts[3]);
		}
	}
}

/**
 * Session thread
 */
typedef struct session_thread_data
{
	int id;
	int interval;
	double duration;
	double actual_duration;
	int failed;
	int detached;
} session_thread_data_t;

/**
 * Check the result of the session thread's test
 * Log a message on failure.  Save the result.
 */
static void check_result(session_thread_data_t *sd)
{
	double threshold = sd->interval / 1000.0f;
	double diff = sd->actual_duration - sd->duration;
	if (diff < 0) {
		diff = diff * -1.0f;
	}
	if (diff > threshold * 2.0) {
		sd->failed = 2;
	} else if (diff > threshold) {
		sd->failed = 1;
	} else {
		sd->failed = 0;
	}
	if (sd->failed > 1) {
		printf("thread #%d FAILED : expected duration = %f, actual duration = %f, diff = %f, threshold = %f\n", sd->id, sd->duration, sd->actual_duration, diff, threshold);
	} else {
		//printf("thread #%d PASSED : expected duration = %f, actual duration = %f, diff = %f, threshold = %f\n", sd->id, sd->duration, sd->actual_duration, diff, threshold);

	}
}

/**
 * Creates a timer and advances it until duration expires
 */
void *session_thread(void *arg)
{
	int *pass = 0;
	session_thread_data_t *d = (session_thread_data_t *)arg;
	switch_timer_t timer = { 0 };

	/* start the timer */
	timer.interval = d->interval;
	timer.samples = d->interval * 8;
	if (timer_if->timer_init(&timer) != SWITCH_STATUS_SUCCESS) {
		printf("WTF!\n");
		goto done;
	}
	//timer_if->timer_sync(&timer);

	/* tick for duration */
	{
		int i;
		struct timespec start, end;
		int ticks = floor(d->duration * 1000 / d->interval);
		clock_gettime(CLOCK_MONOTONIC, &start);
		for (i = 0; i < ticks && !shutdown; i++) {
			timer_if->timer_next(&timer);
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
		d->actual_duration = timespec_subtract(&end, &start);
	}

	/* stop the timer */
	timer_if->timer_destroy(&timer);

	if (!shutdown) {	
		check_result(d);
	}

	pthread_mutex_lock(&session_mutex);
	if (d->failed > 1) {
		fail_count++;
	} else if (d->failed > 0) {
		warn_count++;
	} else {
		pass_count++;
	}
	session_count--;
	if (session_count % 100 == 0 && last_reported_session_count != session_count) {
		printf("sessions = %d\n", session_count);
		last_reported_session_count = session_count;
	}
	pthread_mutex_unlock(&session_mutex);

done:
	if (d->detached) {
		free(d);
		return NULL;
	}
	
	/* return result */
	return d;
}


/**
 * @param thread the thread
 * @param id for logging
 * @param interval the timer period in ms
 * @param duration_mean the mean duration for this thread to execute
 * @param duration_std_dev the standard deviation from the mean duration
 * @param detached if true this thread is detached
 */
static void create_session_thread(pthread_t *thread, int id, int interval, double duration_mean, double duration_std_dev, int detached)
{
	session_thread_data_t *d = malloc(sizeof(session_thread_data_t));
	pthread_mutex_lock(&session_mutex);
	total_sessions++;
	session_count++;
	if (total_sessions % 100 == 0) {
		printf("total sessions = %d, sessions = %d, pass = %d, warn = %d, fail = %d\n", total_sessions, session_count, pass_count, warn_count, fail_count);
	}
	if (session_count % 100 == 0 && last_reported_session_count != session_count) {
		printf("sessions = %d\n", session_count);
		last_reported_session_count = session_count;
	}
	pthread_mutex_unlock(&session_mutex);
	if (interval == 0) {
		printf("WTF WTF WTF!!\n");
		printf("id = %d, interval = %d, duration_mean = %f, duration_std_dev = %f, detached = %d\n", id, interval, duration_mean, duration_std_dev, detached);
	}
	d->id = id;
	d->interval = interval;
	d->duration = randnorm(duration_mean, duration_std_dev);
	/* truncate duration to interval tick */
	d->duration = ceil(d->duration * 1000 / interval) * interval / 1000.0f;
	d->detached = detached;
	d->failed = 0;
	pthread_create(thread, NULL, session_thread, d);
	if (detached) {
		pthread_detach(*thread);
	}
}



/**
 * Create timers at a rate of CPS for test_duration.
 *
 * @param interval array of timer intervals in ms
 * @param interval_weights array of timer intervals weights
 * @param num_intervals size of interval array
 * @param test_duration how long to run this test, in seconds
 * @param cps the "calls per second".  This is the rate at which session threads are created
 * @param duration_mean mean duration for each thread
 * @param duration_std_dev standard deviation from the mean duration
 * @param num_timers number of threads to create
 */
static void test_timer_session(int *interval, int *interval_weights, int num_intervals, double test_duration, int cps, int max_sessions, double duration_mean, double duration_std_dev)
{
	int i = 0;
	struct timespec start, now, period;
	double elapsed = 0.0f;
	
	printf("test_timer_session(%d, %f, %d, %d, %f, %f)\n", interval[0], test_duration, cps, max_sessions, duration_mean, duration_std_dev);


	/* create new call threads at CPS for test_duration */
	if (cps == 1) {
		period.tv_sec = 1;
		period.tv_nsec = 0;
	} else {
		period.tv_sec = 0;
		period.tv_nsec = 1000000000 / cps;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	while (elapsed < test_duration) {
		pthread_t thread;
		int retval = clock_nanosleep(CLOCK_MONOTONIC, 0, &period, NULL);
		if (retval == -1) {
			if (errno == EINTR) {
				/* retry */
				continue;
			}
			printf("clock_nanosleep() error: %s\n", strerror(errno));
			break;
		}
		pthread_mutex_lock(&session_mutex);
		if (session_count < max_sessions) {
			pthread_mutex_unlock(&session_mutex);
			create_session_thread(&thread, ++i, interval[sample(interval_weights, 4)], duration_mean, duration_std_dev, 1);
		} else {
			pthread_mutex_unlock(&session_mutex);
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
		elapsed = timespec_subtract(&now, &start);
	}

	pthread_mutex_lock(&session_mutex);
	while (session_count) {
		struct timespec t;
		t.tv_sec = 0;
		t.tv_nsec = 200 * 1000;
		pthread_mutex_unlock(&session_mutex);
		clock_nanosleep(CLOCK_MONOTONIC, 0, &t, NULL);
		pthread_mutex_lock(&session_mutex);
	}
	pthread_mutex_unlock(&session_mutex);


	printf("test_timer_session(%d, %f, %d, %d, %f, %f) done\n", interval[0], test_duration, cps, max_sessions, duration_mean, duration_std_dev);
}

/**
 * Create num_timers in threads and tick until duration_mean elapses.
 *
 * @param interval timer interval in ms
 * @param duration_mean mean duration for each thread
 * @param duration_std_dev standard deviation from the mean duration
 * @param num_timers number of threads to create
 */
static void test_timer(int interval, double duration_mean, double duration_std_dev, int num_timers)
{
	int i;
	int pass = 1;
	pthread_t *threads = malloc(sizeof(pthread_t) * num_timers);
	printf("test_timer(%d, %f, %f, %d)\n", interval, duration_mean, duration_std_dev, num_timers);


	/* create threads */
	for (i = 0; i < num_timers; i++) {
		create_session_thread(&threads[i], i, interval, duration_mean, duration_std_dev, 0);
	}

	/* wait for thread results */
	for (i = 0; i < num_timers; i++) {
		void *d = NULL;
		pthread_join(threads[i], &d);
		if (d) {
			int result;
			session_thread_data_t *sd = (session_thread_data_t *)d;
			pass = pass & (sd->failed < 2);
			free(sd);
		}
	}

	printf("test_timer(%d, %f, %f, %d) : %s\n", interval, duration_mean, duration_std_dev, num_timers, pass ? "PASS" : "FAIL");
	free(threads);
}

/**
 * Main program
 *
 */
int main (int argc, char **argv)
{
	//int intervals[4] = { 10, 20, 30, 40 };
	//int interval_weights[4] = { 2, 95, 97, 100 };
	int intervals[1] = { 20 };
	int interval_weights[1] = { 100 };
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	srand48(ts.tv_nsec);
	if (load_module() == -1) {
		return -1;
	}
	//test_timer(20, 5.0f, .2f, 1000);
	//test_timer_session(intervals, interval_weights, 4, 2  * 86400.0f, 90, 2000, 30.0, 5.0f);
	while(1) {
		/* stop periodically to trigger timer shutdown */
		test_timer_session(intervals, interval_weights, 1, 60, 150, 190 /* 3000 */, 30.0, 5.0f);
	}
	//test_timer(1000, 5.0f, 1);
	//test_timer(20, 5.0f, .2f, 1000);
	//test_timer(30, 5.0f, 1000);
	//test_create_destroy();
	shutdown_module();
	return 0;
}

