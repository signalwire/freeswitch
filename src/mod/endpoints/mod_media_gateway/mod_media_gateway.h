/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/


#ifndef MOD_MEGACO_H
#define MOD_MEGACO_H

#include "sng_mg/sng_mg.h"
#include <switch.h>

#define MG_MAX_PEERS    5

#define MEGACO_CLI_SYNTAX "profile|logging"
#define MEGACO_LOGGING_CLI_SYNTAX "logging [enable|disable]"
#define MEGACO_FUNCTION_SYNTAX "profile [name] [start | stop] [status] [xmlstatus] [peerxmlstatus]"

struct megaco_globals {
	switch_memory_pool_t 		*pool;
	switch_hash_t 			*profile_hash;
	switch_hash_t 			*peer_profile_hash;
	switch_thread_rwlock_t 		*profile_rwlock;
	switch_thread_rwlock_t 		*peer_profile_rwlock;
};
extern struct megaco_globals megaco_globals; /* < defined in mod_megaco.c */

typedef enum {
	PF_RUNNING = (1 << 0)
} megaco_profile_flags_t;

typedef struct mg_peer_profile_s{
	char 				*name;
	switch_memory_pool_t 		*pool;
	switch_thread_rwlock_t 		*rwlock; /* < Reference counting rwlock */
	megaco_profile_flags_t 		flags;
	char*  				ipaddr;      /* Peer IP  */
  	char* 				port;        /*Peer Port */
	char*       			mid;  	     /* Peer H.248 MID */
	char*       			transport_type; /* UDP/TCP */ 
	char*      			encoding_type; /* Encoding TEXT/Binary */
}mg_peer_profile_t;


typedef struct megaco_profile_s {
	char 				*name;
	switch_memory_pool_t 		*pool;
	switch_thread_rwlock_t 		*rwlock; /* < Reference counting rwlock */
	megaco_profile_flags_t 		flags;
	int 				idx;         /* Trillium MEGACO SAP identification*/
	char*                 		mid;  	     /* MG H.248 MID */
	char*                 		my_domain;   /* local domain name */
	char*                 		my_ipaddr;   /* local domain name */
	char*                		port;              	     /* port */
	char*                		protocol_type;    	     /* MEGACO/MGCP */
	int 				protocol_version;            /* Protocol supported version */
	int 				total_peers;            
	char*                		peer_list[MG_MAX_PEERS];     /* MGC Peer ID LIST */
} megaco_profile_t;


megaco_profile_t *megaco_profile_locate(const char *name);
mg_peer_profile_t *megaco_peer_profile_locate(const char *name);
void megaco_profile_release(megaco_profile_t *profile);

switch_status_t megaco_profile_start(const char *profilename);
switch_status_t megaco_profile_destroy(megaco_profile_t **profile);
switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload);
switch_status_t sng_mgco_start(megaco_profile_t* profile);
switch_status_t sng_mgco_stop(megaco_profile_t* profile);
switch_status_t mg_config_cleanup(megaco_profile_t* profile);
switch_status_t mg_process_cli_cmd(const char *cmd, switch_stream_handle_t *stream);


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
