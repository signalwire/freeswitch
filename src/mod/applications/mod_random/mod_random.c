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
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_random.c -- entropy source module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_random_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_random_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_random_load);

SWITCH_MODULE_DEFINITION(mod_random, mod_random_load, mod_random_shutdown, mod_random_runtime);

static int RUNNING = 0;
static const char *random_device_files[] = { "/dev/hwrandom", "/dev/random", NULL };
const char *random_device_file = NULL;

static void event_handler(switch_event_t *event);


SWITCH_MODULE_LOAD_FUNCTION(mod_random_load)
{

#ifdef WIN32
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s missing window support\n", modname);
	return SWITCH_STATUS_NOTIMPL;
#endif

	int i = 0;


	for(i = 0 ;random_device_files[i]; i++) {
		if (switch_file_exists(random_device_files[i], pool) == SWITCH_STATUS_SUCCESS) {
			random_device_file = random_device_files[i];
			break;
		}
	}


	if (!random_device_file) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s can't locate a random device file\n", modname);
		return SWITCH_STATUS_FALSE;
	}


	if ((switch_event_bind(modname, SWITCH_EVENT_ALL, NULL, event_handler, NULL) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	RUNNING = 1;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_random_shutdown)
{

	switch_event_unbind_callback(event_handler);

	RUNNING = 0;

	return SWITCH_STATUS_SUCCESS;
}

#if WIN32
SWITCH_MODULE_RUNTIME_FUNCTION(mod_random_runtime)
{
	RUNNING = 0;
	return SWITCH_STATUS_TERM;
}
#else

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <linux/types.h>
#include <linux/random.h>
#include <string.h>



typedef struct {
	int count;
	int size;
	unsigned char *data;
} entropy_t;


static int random_add_entropy(int fd, void *buf, size_t size)
{
	entropy_t e = { 0 };
	int r = 0;

	e.count = size * 8;
	e.size = size;
	e.data = (unsigned char *) buf;

	if (ioctl(fd, RNDADDENTROPY, &e) != 0) {
		r = 1;
	}

	return r;
}

static int rng_read(int fd, void *buf, size_t size)
{
	size_t off = 0;
	ssize_t r;
	unsigned char *bp = (unsigned char *) buf;

	while (size > 0) {
		do {
			r = read(fd, bp + off, size);
		} while ((r == -1) && (errno == EINTR));

		if (r <= 0) {
			break;
		}

		off += r;
		size -= r;
	}

	return size;
}

static int rfd = 0;

static void event_handler(switch_event_t *event)
{
	char *buf;

	if (switch_event_serialize(event, &buf, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
		random_add_entropy(rfd, buf, strlen(buf));  
		free(buf);
	}

}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_random_runtime)
{

	unsigned char data[1024] = {0};
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Thread starting using random_device_file %s\n", modname, random_device_file); 

	if ((rfd = open(random_device_file, O_RDWR)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Error opening random_device_file %s\n", modname, random_device_file); 
		RUNNING = 0;
	}

	rng_read(rfd, data, 4);
	
	while(RUNNING) {
		int16_t data[64];
		int i = 0;
		int len = sizeof(data) / 2;

		switch_generate_sln_silence(data, len, 1);
		random_add_entropy(rfd, data, len);	

		while(i < len && !data[i]) i++;

		if (i < len) {
			switch_yield(abs(data[i]) * 1000);
		}

	}

	if (rfd > -1) {
		close(rfd);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Thread ending\n", modname); 

	return SWITCH_STATUS_TERM;
}

#endif


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
