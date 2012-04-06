
#include <switch.h>
#include <stdlib.h>

extern SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load);
extern SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown);

switch_loadable_module_interface_t *mod = NULL;
switch_memory_pool_t pool = { 0 };

int main (int argc, char **argv)
{
	int i;
	switch_timer_interface_t *timer_if;
	switch_timer_t *timer[1000];

	mod_posix_timer_load(&mod, &pool);
	timer_if = mod->timer;


	// TODO create multi-threaded test

	// create 10 ms timers
	for (i = 0; i < 1000; i++) {
		timer[i] = malloc(sizeof(switch_timer_t));
		memset(timer[i], 0, sizeof(switch_timer_t));
		timer[i]->interval = 1;
		timer[i]->samples = 8;
		timer_if->timer_init(timer[i]);
	}

	for (i = 0; i < 50000; i++) {
		timer_if->timer_next(timer[0]);
	}

	// destroy timers
	for (i = 0; i < 1000; i++) {
		timer_if->timer_destroy(timer[i]);
		free(timer[i]);
	}

	mod_posix_timer_shutdown();
	return 0;
}
