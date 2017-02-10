/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
*
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* William King <william.king@quentustech.com>
*
* mod_sms_flowroute.c SMS support for Flowroute SMS
*
*/

#ifndef MOD_SMS_FLOWROUTE_H
#define MOD_SMS_FLOWROUTE_H

#define H2O_USE_LIBUV 0
#define H2O_USE_BROTLI 1

#include <switch.h>
#include "h2o.h"
#include "h2o/http1client.h"

typedef struct {
  char *name;
  int port;
  int debug;
  int running;
  char *host;

  char *access_key;
  char *secret_key;

  unsigned char auth_b64[512];
  int auth_b64_size;

  h2o_url_t url_parsed;
  h2o_socketpool_t *sockpool;

  h2o_globalconf_t h2o_globalconf;
  h2o_hostconf_t *h2o_hostconf;
  h2o_pathconf_t *h2o_pathconf;
  h2o_handler_t *h2o_handler;
  h2o_context_t h2o_context;
  h2o_accept_ctx_t *h2o_accept_context;
  h2o_multithread_queue_t *queue;

  switch_thread_t *profile_thread;
  switch_memory_pool_t *pool;
} mod_sms_flowroute_profile_t;

typedef struct {
  h2o_http1client_ctx_t ctx;
  mod_sms_flowroute_profile_t *profile;
  switch_mutex_t *mutex;
  h2o_iovec_t req;
  int status;
  h2o_multithread_receiver_t getaddr_receiver;
  h2o_timeout_t io_timeout;
} mod_sms_flowroute_message_t;

typedef struct mod_sms_flowroute_globals_s {
  switch_memory_pool_t *pool;
  switch_hash_t *profile_hash;
  int debug;
} mod_sms_flowroute_globals_t;

extern mod_sms_flowroute_globals_t mod_sms_flowroute_globals;


#endif /* MOD_SMS_FLOWROUTE_H */

