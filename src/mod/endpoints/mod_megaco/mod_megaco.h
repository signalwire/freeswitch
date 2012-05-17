/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/


#ifndef MOD_MEGACO_H
#define MOD_MEGACO_H

#include <switch.h>
#include "megaco_stack.h"

struct megaco_globals {
	switch_memory_pool_t 		*pool;
	switch_hash_t 			*profile_hash;
	switch_thread_rwlock_t 		*profile_rwlock;
	sng_mg_gbl_cfg_t 		 g_mg_cfg;
};
extern struct megaco_globals megaco_globals; /* < defined in mod_megaco.c */

typedef enum {
	PF_RUNNING = (1 << 0)
} megaco_profile_flags_t;

typedef struct megaco_profile_s {
	char 				*name;
	switch_memory_pool_t 		*pool;
	switch_thread_rwlock_t 		*rwlock; /* < Reference counting rwlock */
	megaco_profile_flags_t 		flags;
} megaco_profile_t;


megaco_profile_t *megaco_profile_locate(const char *name);
void megaco_profile_release(megaco_profile_t *profile);

switch_status_t megaco_profile_start(const char *profilename);
switch_status_t megaco_profile_destroy(megaco_profile_t **profile);


#endif /* MOD_MEGACO_H */


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
