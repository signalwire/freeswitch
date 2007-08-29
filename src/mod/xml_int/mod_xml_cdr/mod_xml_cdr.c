/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Brian West <brian.west@mac.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 * mod_xml_cdr.c -- XML CDR Module to files or curl
 *
 */
#include <sys/stat.h>
#include <switch.h>
#include <curl/curl.h>

static struct {
	char *cred;
	char *url;
	char *logDir;
	char *errLogDir;
	uint32_t delay;
	uint32_t retries;
	uint32_t shutdown;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_cdr, mod_xml_cdr_load, NULL, NULL);

/* this function would have access to the HTML returned by the webserver, we dont need it 
 * and the default curl activity is to print to stdout, something not as desirable
 * so we have a dummy function here
 */
static void httpCallBack()
{
	return;
}

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_xml_t cdr;
	char *xml_text;
	char *path;
	int fd = -1;
	uint32_t curTry;
	long httpRes;
	CURL *curl_handle = NULL;
	char *curl_xml_text;
	char *logdir;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	uint32_t i;

	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {

		/* build the XML */
		if(!(xml_text = switch_mprintf("<?xml version=\"1.0\"?>\n%s",switch_xml_toxml(cdr)) )) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		/* do we log to the disk no matter what? */
		/* all previous functionality is retained */

		if (!(logdir = switch_channel_get_variable(channel, "xml_cdr_base"))) {
			logdir = globals.logDir;
		}

		if(!switch_strlen_zero(logdir)) {
			if ((path = switch_mprintf("%s/xml_cdr/%s.cdr.xml", logdir, switch_core_session_get_uuid(session)))) {
				if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
					int wrote;
					wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
					close(fd);
					fd = -1;
				} else {
					char ebuf[512] = { 0 };
#ifdef WIN32
					strerror_s(ebuf, sizeof(ebuf), errno);
#else
					strerror_r(errno, ebuf, sizeof(ebuf));
#endif
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n", ebuf);
				}
				free(path);
			}
		}

		/* try to post it to the web server */
		if(!switch_strlen_zero(globals.url)) {
			curl_handle = curl_easy_init();
			if(!(curl_xml_text = switch_mprintf("cdr=%s",xml_text) )) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				free(xml_text);
				return SWITCH_STATUS_FALSE;
			}

			if (!switch_strlen_zero(globals.cred)) {
				curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
				curl_easy_setopt(curl_handle, CURLOPT_USERPWD, globals.cred);
			}

			curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, curl_xml_text);
			curl_easy_setopt(curl_handle, CURLOPT_URL, globals.url);
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpCallBack);

			/* these were used for testing, optionally they may be enabled if someone desires
			curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 120); // tcp timeout
			curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); // 302 recursion level
			*/

			for(curTry=0;curTry<=globals.retries;curTry++) {
				curl_easy_perform(curl_handle);
				curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE,&httpRes);
				if(httpRes==200) {
					curl_easy_cleanup(curl_handle);
					free(curl_xml_text);
					free(xml_text);
					return SWITCH_STATUS_SUCCESS;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got error [%ld] posting to web server\n",httpRes);
				}

				/* make sure we dont sleep on the last try */
				for(i=0;i<globals.delay && (curTry != (globals.retries));i++) {
					switch_sleep(1000);
					if(globals.shutdown) {
						/* we are shutting down so dont try to webpost any more */
						i=globals.delay;
						curTry=globals.retries;
					}
				}
						
			}
			free(curl_xml_text);
			curl_easy_cleanup(curl_handle);

			/* if we are here the web post failed for some reason */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to post to web server, writing to file\n");

			if ((path = switch_mprintf("%s/%s.cdr.xml", globals.errLogDir, switch_core_session_get_uuid(session)))) {
				if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
					int wrote;
					wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
					close(fd);
					fd = -1;
				} else {
					char ebuf[512] = { 0 };
#ifdef WIN32
					strerror_s(ebuf, sizeof(ebuf), errno);
#else
					strerror_r(errno, ebuf, sizeof(ebuf));
#endif
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n", ebuf);
				}
				free(path);
			}
		}
		free(xml_text);
		switch_xml_free(cdr);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating Data!\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_cdr_load)
{
	char *cf = "xml_cdr.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals,0,sizeof(globals));

	/* parse the config */
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}
	
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "cred")) {
				globals.cred = val;
			} else if (!strcasecmp(var, "url")) {
				globals.url = val;
			} else if (!strcasecmp(var, "delay")) {
				globals.delay = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "retries")) {
				globals.retries = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "logDir")) {
				if (switch_strlen_zero(val)) {
					globals.logDir = SWITCH_GLOBAL_dirs.log_dir;
				} else {
					globals.logDir = val;
				}
			} else if (!strcasecmp(var, "errLogDir")) {
				globals.errLogDir = val;
			}

		}
	}
	if(globals.retries < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "retries is negative, setting to 0\n");
		globals.retries = 0;
	}


	if(globals.retries && globals.delay<=0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "retries set but delay 0 setting to 5000ms\n");
		globals.delay = 5000;
	}

	if(!switch_strlen_zero(globals.url) && switch_strlen_zero(globals.errLogDir)) {
		if ((globals.errLogDir = switch_mprintf("%s/xml_cdr", SWITCH_GLOBAL_dirs.log_dir))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}
	}


 done:
	switch_xml_free(xml);
	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown)
{
	
	globals.shutdown=1;
    return SWITCH_STATUS_SUCCESS;
}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
