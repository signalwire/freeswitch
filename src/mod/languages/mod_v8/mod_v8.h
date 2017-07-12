/*
 * mod_v8 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Peter Olsson <peter@olssononline.se>
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
 * The Original Code is ported from FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Andrey Volk <andywolk@gmail.com>
 *
 * mod_v8.h -- JavaScript FreeSWITCH module header file
 *
 */

#ifndef MOD_V8_H
#define MOD_V8_H

#include "javascript.hpp"
#include <switch.h>

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
void LoadScript(v8::MaybeLocal<v8::Script> *v8_script, v8::Isolate *isolate, const char *script_data, const char *script_file);
#endif

SWITCH_BEGIN_EXTERN_C

#define JS_BUFFER_SIZE 1024 * 32
#define JS_BLOCK_SIZE JS_BUFFER_SIZE

/* Function definition for initialization of an extension module */
typedef switch_status_t (*v8_mod_load_t) (const v8::FunctionCallbackInfo<v8::Value>& info);

/* Extension module interface, stored inside the load_hash */
typedef struct {
	const char *name;
	v8_mod_load_t v8_mod_load;
} v8_mod_interface_t;

/* Function definition for external extension module */
typedef switch_status_t (*v8_mod_init_t) (const v8_mod_interface_t **module_interface);

/* Struct that holds information about loadable extension modules */
typedef struct {
	switch_hash_t *load_hash;
	switch_memory_pool_t *pool;
} module_manager_t;

extern module_manager_t module_manager;

/* Struct that stores XML handler information */
typedef struct {
	const char *section;
	const char *tag_name;
	const char *key_name;
	const char *key_value;
	switch_event_t *params;
	void *user_data;
	char* XML_STRING;
} v8_xml_handler_t;

/* Struct that stores a javascript variable's name for an event */
typedef struct {
	const char* var_name;
	switch_event_t *event;
}
v8_event_t;

SWITCH_END_EXTERN_C

void v8_add_event_handler(void *event_handler);
void v8_remove_event_handler(void *event_handler);

#endif /* MOD_V8_H */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
