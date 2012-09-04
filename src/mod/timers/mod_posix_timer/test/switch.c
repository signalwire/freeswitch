#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


switch_loadable_module_interface_t * switch_loadable_module_create_module_interface(switch_memory_pool_t *pool, const char *name)
{
	return malloc(sizeof(switch_loadable_module_interface_t));
}

void * switch_loadable_module_create_interface(switch_loadable_module_interface_t *mod, int iname)
{
	mod->timer = malloc(sizeof(switch_timer_interface_t));
	return mod->timer;
}

switch_status_t switch_mutex_lock(switch_mutex_t *mutex)
{
	return pthread_mutex_lock(mutex);
}

switch_status_t switch_mutex_unlock(switch_mutex_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

switch_status_t switch_mutex_init(switch_mutex_t **mutex, int flags, switch_memory_pool_t *pool)
{
	pthread_mutexattr_t atts = { 0 };
	pthread_mutexattr_init(&atts);
	if (flags == SWITCH_MUTEX_NESTED) {
		pthread_mutexattr_settype(&atts, PTHREAD_MUTEX_RECURSIVE_NP);
	}
	*mutex = malloc(sizeof(switch_mutex_t));
	return pthread_mutex_init(*mutex, &atts);
}

switch_status_t switch_thread_cond_create(switch_thread_cond_t **cond, switch_memory_pool_t *pool)
{
	*cond = malloc(sizeof(switch_thread_cond_t));
	return pthread_cond_init(*cond, NULL);
}

switch_status_t switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_mutex_t *mutex, long wait)
{
	struct timespec abs_time = { 0, 0 };
	/* add wait duration to current time (wait is in microseconds, pthreads wants nanosecond resolution) */
	clock_gettime(CLOCK_REALTIME, &abs_time);
	abs_time.tv_sec += wait / 1000000;
	abs_time.tv_nsec += (wait % 1000000) * 1000;
	/* handle overflow of tv_nsec */
	abs_time.tv_sec += abs_time.tv_nsec / 1000000000;
	abs_time.tv_nsec = abs_time.tv_nsec % 1000000000;
	return pthread_cond_timedwait(cond, mutex, &abs_time);
}

switch_status_t switch_thread_cond_broadcast(switch_thread_cond_t *cond)
{
	return pthread_cond_broadcast(cond);
}

void switch_log_printf(int dummy, int level, char *format, ...)
{
	va_list vl;
	va_start(vl, format);
	if (level > LOG_LEVEL) {
		vprintf(format, vl);
	}
	va_end(vl);
}


