#ifndef SWITCH_H
#define SWITCH_H 

#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define SWITCH_STATUS_SUCCESS 0
#define SWITCH_STATUS_GENERR 1
#define SWITCH_STATUS_FALSE 2
#define SWITCH_STATUS_TERM 3

#define SWITCH_MUTEX_NESTED 1

#define SWITCH_CHANNEL_LOG 0

#define SWITCH_LOG_DEBUG 0
#define SWITCH_LOG_INFO 0
#define SWITCH_LOG_ERROR 1

typedef int switch_status_t;
typedef size_t switch_size_t;
typedef pthread_mutex_t switch_mutex_t;
typedef pthread_cond_t switch_thread_cond_t;
typedef int switch_memory_pool_t;
typedef int switch_bool_t;

#define SWITCH_TIMER_INTERFACE 0

typedef struct switch_loadable_module_interface switch_loadable_module_interface_t;
typedef struct switch_timer_interface switch_timer_interface_t;

typedef int switch_module_flag_t;
#define SWITCH_API_VERSION 0
#define SWITCH_MOD_DECLARE_DATA
#define SMODF_NONE 0
#define SWITCH_MODULE_LOAD_ARGS (switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
#define SWITCH_MODULE_RUNTIME_ARGS (void)
#define SWITCH_MODULE_SHUTDOWN_ARGS (void)
typedef switch_status_t (*switch_module_load_t) SWITCH_MODULE_LOAD_ARGS;
typedef switch_status_t (*switch_module_runtime_t) SWITCH_MODULE_RUNTIME_ARGS;
typedef switch_status_t (*switch_module_shutdown_t) SWITCH_MODULE_SHUTDOWN_ARGS;
#define SWITCH_MODULE_LOAD_FUNCTION(name) switch_status_t name SWITCH_MODULE_LOAD_ARGS
#define SWITCH_MODULE_RUNTIME_FUNCTION(name) switch_status_t name SWITCH_MODULE_RUNTIME_ARGS
#define SWITCH_MODULE_SHUTDOWN_FUNCTION(name) switch_status_t name SWITCH_MODULE_SHUTDOWN_ARGS
typedef struct switch_loadable_module_function_table {
    int switch_api_version;
    switch_module_load_t load;
    switch_module_shutdown_t shutdown;
    switch_module_runtime_t runtime;
    switch_module_flag_t flags;
} switch_loadable_module_function_table_t;

#define SWITCH_MODULE_DEFINITION_EX(name, load, shutdown, runtime, flags)                   \
static const char modname[] =  #name ;                                                      \
SWITCH_MOD_DECLARE_DATA switch_loadable_module_function_table_t name##_module_interface = { \
    SWITCH_API_VERSION,                                                                     \
    load,                                                                                   \
    shutdown,                                                                               \
    runtime,                                                                                \
    flags                                                                                   \
}

#define SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)                             \
        SWITCH_MODULE_DEFINITION_EX(name, load, shutdown, runtime, SMODF_NONE)



switch_loadable_module_interface_t * switch_loadable_module_create_module_interface(switch_memory_pool_t *pool, const char *name);

typedef struct {
	int id;
	int interval;
	int tick;
	int samplecount;
	int samples;
	int diff;
	void *private_info;
} switch_timer_t;


/*! \brief A table of functions that a timer module implements */
struct switch_timer_interface {
	/*! the name of the interface */
	const char *interface_name;
	/*! function to allocate the timer */
	switch_status_t (*timer_init) (switch_timer_t *);
	/*! function to wait for one cycle to pass */
	switch_status_t (*timer_next) (switch_timer_t *);
	/*! function to step the timer one step */
	switch_status_t (*timer_step) (switch_timer_t *);
	/*! function to reset the timer  */
	switch_status_t (*timer_sync) (switch_timer_t *);
	/*! function to check if the current step has expired */
	switch_status_t (*timer_check) (switch_timer_t *, switch_bool_t);
	/*! function to deallocate the timer */
	switch_status_t (*timer_destroy) (switch_timer_t *);
	int refs;
	switch_mutex_t *reflock;
	switch_loadable_module_interface_t *parent;
	struct switch_timer_interface *next;
};

struct switch_loadable_module_interface {
	switch_timer_interface_t *timer;
};

void * switch_loadable_module_create_interface(switch_loadable_module_interface_t *mod, int iname);

switch_status_t switch_mutex_lock(switch_mutex_t *mutex);

switch_status_t switch_mutex_unlock(switch_mutex_t *mutex);

switch_status_t switch_mutex_init(switch_mutex_t **mutex, int flags, switch_memory_pool_t *pool);

switch_status_t switch_thread_cond_create(switch_thread_cond_t **cond, switch_memory_pool_t *pool);

switch_status_t switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_mutex_t *mutex, long wait);

switch_status_t switch_thread_cond_broadcast(switch_thread_cond_t *cond);

void switch_log_printf(int dummy, int level, char *format, ...);

#endif
