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
 *
 *
 * mod_xml_cdr.c -- XML CDR Module
 *
 */
#include <sys/stat.h>
#include <switch.h>

static const char modname[] = "mod_xml_cdr";

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_xml_t cdr;
	
	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *xml_text;
		char *path;
		int fd = -1;
		const char *header = "<?xml version=\"1.0\"?>\n";
		char *uuid_str = switch_core_session_get_uuid(session);
		char *logdir = SWITCH_GLOBAL_dirs.log_dir;
		char *alt;

		if ((alt = switch_channel_get_variable(channel, "xml_cdr_base"))) {
			logdir = alt;
		}
		
		if ((path = switch_mprintf("%s/xml_cdr/%s.cdr.xml", logdir, uuid_str))) {
			if ((xml_text = switch_xml_toxml(cdr))) {
				if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR)) > -1) {
					write(fd, header, (unsigned)strlen(header));
					write(fd, xml_text, (unsigned)strlen(xml_text));
					close(fd);
					fd = -1;
				} else {
					char ebuf[512] = {0};
#ifdef WIN32
					strerror_s(ebuf, sizeof(ebuf), errno);
#else
					strerror_r(errno, ebuf, sizeof(ebuf));
#endif
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n", ebuf);
				}
				free(xml_text);
			}
			free(path);
		}

		switch_xml_free(cdr);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating Data!\n");
	}
	
	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};


static const switch_loadable_module_interface_t mod_xml_cdr_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	*module_interface = &mod_xml_cdr_module_interface;

	/* indicate that the module should continue to be loaded */
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
