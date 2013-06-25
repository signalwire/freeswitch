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
 *
 * mod_ladspa.c -- LADSPA 
 *
 */
#include <switch.h>
#include "ladspa.h"
#include "utils.h"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ladspa_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_ladspa_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_ladspa_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_ladspa, mod_ladspa_load, mod_ladspa_shutdown, NULL);

#define MAX_INDEX 256

typedef struct {
	switch_core_session_t *session;
	char *plugin_name;
	char *label_name;
	void *library_handle;
	const LADSPA_Descriptor *ldesc;
	LADSPA_Handle handle;
	LADSPA_Data config[MAX_INDEX];
	int num_idx;
	char *str_config[MAX_INDEX];
	int str_idx;
	uint8_t has_config[MAX_INDEX];
	int skip;
	LADSPA_Data in_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	LADSPA_Data file_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	LADSPA_Data out_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	LADSPA_Data out_ports[MAX_INDEX];
	switch_file_handle_t fh;
} switch_ladspa_t;



int check_range(const LADSPA_Descriptor *ldesc, int i, LADSPA_Data val)
{
	if (ldesc->PortRangeHints[i].LowerBound && ldesc->PortRangeHints[i].UpperBound && 
		(val < ldesc->PortRangeHints[i].LowerBound || val > ldesc->PortRangeHints[i].UpperBound)) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Param %f out of bounds %f-%f\n", 
						  val, ldesc->PortRangeHints[i].LowerBound, ldesc->PortRangeHints[i].UpperBound);
		return 0;
	}

	return 1;
}

int find_default(const LADSPA_Descriptor *ldesc, int i, LADSPA_Data *ptr)
						 
{
	LADSPA_Data dftval = 0;
	int fail = 0;

	LADSPA_PortRangeHintDescriptor port_hint = ldesc->PortRangeHints[i].HintDescriptor;
	
	switch (port_hint & LADSPA_HINT_DEFAULT_MASK) {
	case LADSPA_HINT_DEFAULT_NONE:
		break;
	case LADSPA_HINT_DEFAULT_MINIMUM:
		dftval = ldesc->PortRangeHints[i].LowerBound;
		break;
	case LADSPA_HINT_DEFAULT_LOW:
		if (LADSPA_IS_HINT_LOGARITHMIC(port_hint)) {
			dftval = exp(log(ldesc->PortRangeHints[i].LowerBound)
						 * 0.75 + log(ldesc->PortRangeHints[i].UpperBound)
						 * 0.25);
		} else {
			dftval = (ldesc->PortRangeHints[i].LowerBound * 0.75 + ldesc->PortRangeHints[i].UpperBound * 0.25);
		}
		break;
	case LADSPA_HINT_DEFAULT_MIDDLE:
		if (LADSPA_IS_HINT_LOGARITHMIC(port_hint)) {
			dftval = sqrt(ldesc->PortRangeHints[i].LowerBound * ldesc->PortRangeHints[i].UpperBound);
		} else {
			dftval = 0.5 * (ldesc->PortRangeHints[i].LowerBound + ldesc->PortRangeHints[i].UpperBound);
		}
		break;
	case LADSPA_HINT_DEFAULT_HIGH:
		if (LADSPA_IS_HINT_LOGARITHMIC(port_hint)) {
			dftval = exp(log(ldesc->PortRangeHints[i].LowerBound)
						 * 0.25 + log(ldesc->PortRangeHints[i].UpperBound)
						 * 0.75);
		} else {
			dftval = (ldesc->PortRangeHints[i].LowerBound * 0.25 + ldesc->PortRangeHints[i].UpperBound * 0.75);
		}
		break;
	case LADSPA_HINT_DEFAULT_MAXIMUM:
		dftval = ldesc->PortRangeHints[i].UpperBound;
		break;
	case LADSPA_HINT_DEFAULT_0:
		dftval = 0;
		break;
	case LADSPA_HINT_DEFAULT_1:
		dftval = 1;
		break;
	case LADSPA_HINT_DEFAULT_100:
		dftval = 100;
		break;
	case LADSPA_HINT_DEFAULT_440:
		dftval = 440;
		break;
	default:
		fail = 1;
		break;
	}

	if (!fail) {
		*ptr = dftval;
	}

	return !fail;
}

static void dump_info(const LADSPA_Descriptor *ldesc)
{
	int i = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Plugin Name: \"%s\"\n", ldesc->Name);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Plugin Label: \"%s\"\n", ldesc->Label);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Plugin Unique ID: %lu\n", ldesc->UniqueID);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Maker: \"%s\"\n", ldesc->Maker);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Copyright: \"%s\"\n", ldesc->Copyright);

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Must Run Real-Time: ");
	if (LADSPA_IS_REALTIME(ldesc->Properties))
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Yes\n");
	else
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "No\n");

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Has activate() Function: ");
	if (ldesc->activate != NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Yes\n");
	else
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "No\n");
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Has deactivate() Function: ");
	if (ldesc->deactivate != NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Yes\n");
	else
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "No\n");
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Has run_adding() Function: ");
	if (ldesc->run_adding != NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Yes\n");
	else
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "No\n");

	if (ldesc->instantiate == NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS NO INSTANTIATE FUNCTION.\n");
	if (ldesc->connect_port == NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS NO CONNECT_PORT FUNCTION.\n");
	if (ldesc->run == NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS NO RUN FUNCTION.\n");
	if (ldesc->run_adding != NULL && ldesc->set_run_adding_gain == NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS RUN_ADDING FUNCTION BUT " "NOT SET_RUN_ADDING_GAIN.\n");
	if (ldesc->run_adding == NULL && ldesc->set_run_adding_gain != NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS SET_RUN_ADDING_GAIN FUNCTION BUT " "NOT RUN_ADDING.\n");
	if (ldesc->cleanup == NULL)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: PLUGIN HAS NO CLEANUP FUNCTION.\n");

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Environment: ");
	if (LADSPA_IS_HARD_RT_CAPABLE(ldesc->Properties))
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Normal or Hard Real-Time\n");
	else
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Normal\n");

	if (LADSPA_IS_INPLACE_BROKEN(ldesc->Properties))
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "This plugin cannot use in-place processing. " "It will not work with all hosts.\n");

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "Ports:");

	if (ldesc->PortCount == 0)
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "\tERROR: PLUGIN HAS NO PORTS.\n");

	for (i = 0; i < ldesc->PortCount; i++) {
		LADSPA_Data dft = 0.0f;
		int found = 0;

		if (LADSPA_IS_PORT_CONTROL(ldesc->PortDescriptors[i])) {
			found = find_default(ldesc, i, &dft);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "\n  \"%s\" ", ldesc->PortNames[i]);

		if (LADSPA_IS_PORT_INPUT(ldesc->PortDescriptors[i])
			&& LADSPA_IS_PORT_OUTPUT(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: INPUT AND OUTPUT");
		else if (LADSPA_IS_PORT_INPUT(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "input");
		else if (LADSPA_IS_PORT_OUTPUT(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "output");
		else
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "ERROR: NEITHER INPUT NOR OUTPUT");

		if (LADSPA_IS_PORT_CONTROL(ldesc->PortDescriptors[i])
			&& LADSPA_IS_PORT_AUDIO(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, ", ERROR: CONTROL AND AUDIO");
		else if (LADSPA_IS_PORT_CONTROL(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, ", control");
		else if (LADSPA_IS_PORT_AUDIO(ldesc->PortDescriptors[i]))
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, ", audio");
		else
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, ", ERROR: NEITHER CONTROL NOR AUDIO");

		if (LADSPA_IS_PORT_CONTROL(ldesc->PortDescriptors[i])) {
			if (found) {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "\n    RANGE: %f-%f DEFAULT: %f\n", 
								  ldesc->PortRangeHints[i].LowerBound, ldesc->PortRangeHints[i].UpperBound, dft);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "\n    RANGE: %f-%f DEFAULT: none.\n", 
								  ldesc->PortRangeHints[i].LowerBound, ldesc->PortRangeHints[i].UpperBound);
			}
		}

		

	}

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "\n\n");
}





static switch_bool_t ladspa_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_ladspa_t *pvt = (switch_ladspa_t *) user_data;
	//switch_frame_t *frame = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT: 
		{
			switch_codec_implementation_t read_impl = { 0 };
			LADSPA_PortDescriptor port_desc;
			int i = 0, j = 0, k = 0, str_idx = 0;
			
			switch_core_session_get_read_impl(pvt->session, &read_impl);

			if (!(pvt->library_handle = loadLADSPAPluginLibrary(pvt->plugin_name))) {
				return SWITCH_FALSE;
			}
			
			if (!(pvt->ldesc = findLADSPAPluginDescriptor(pvt->library_handle, pvt->plugin_name, pvt->label_name))) {
				return SWITCH_FALSE;
			}


			pvt->handle = pvt->ldesc->instantiate(pvt->ldesc, read_impl.actual_samples_per_second);

			dump_info(pvt->ldesc);
			

			for (i = 0; i < pvt->ldesc->PortCount; i++) {
				port_desc = pvt->ldesc->PortDescriptors[i];
				
				if (LADSPA_IS_PORT_CONTROL(port_desc) && LADSPA_IS_PORT_INPUT(port_desc)) {
					LADSPA_Data dft = 0.0f;
					int found = find_default(pvt->ldesc, i, &dft);
					
					if (found && !pvt->has_config[j]) {
						pvt->config[j] = dft;
						pvt->has_config[j] = 1;
					}

					if (pvt->has_config[j]) {
						if (!check_range(pvt->ldesc, i, pvt->config[j])) {
							pvt->config[j] = dft;
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_WARNING, "FALLING TO DEFAULT PARAM %d [%s] (%f)\n", 
											  j+1, 
											  pvt->ldesc->PortNames[i],
											  pvt->config[j]);							
						}
						
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "ADDING PARAM %d [%s] (%f)\n", 
										  j+1, 
										  pvt->ldesc->PortNames[i],
										  pvt->config[j]);
						pvt->ldesc->connect_port(pvt->handle, i, &pvt->config[j++]);
						usleep(10000);
					}
				}

				if (LADSPA_IS_PORT_INPUT(port_desc) && LADSPA_IS_PORT_AUDIO(port_desc)) {
					int mapped = 0;

					if (pvt->str_idx && !zstr(pvt->str_config[str_idx])) {

						if (!strcasecmp(pvt->str_config[str_idx], "none")) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "CONNECT NOTHING to port: %s\n", 
											  pvt->ldesc->PortNames[i]
											  );
							mapped = 1;
						} else if (!strncasecmp(pvt->str_config[str_idx], "file:", 5)) {
							char *file = pvt->str_config[str_idx] + 5;

							if (switch_test_flag((&pvt->fh), SWITCH_FILE_OPEN)) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), 
												  SWITCH_LOG_ERROR, "CAN'T CONNECT FILE [%s] File already mapped\n", file);
							} else {
								if (switch_core_file_open(&pvt->fh,
														  file,
														  read_impl.number_of_channels,
														  read_impl.actual_samples_per_second, 
														  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_ERROR, "Cannot open file: %s\n", file);
									return SWITCH_FALSE;
								}
							
							
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "CONNECT FILE [%s] to port: %s\n", 
												  file,
												  pvt->ldesc->PortNames[i]
												  );

								pvt->ldesc->connect_port(pvt->handle, i, pvt->file_buf);
								mapped = 1;
							}
						}

						str_idx++;
					}

					if (!mapped) {
						pvt->ldesc->connect_port(pvt->handle, i, pvt->in_buf);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "CONNECT CHANNEL AUDIO to port: %s\n", 
										  pvt->ldesc->PortNames[i]
										  );
					}

				}

				if (LADSPA_IS_PORT_OUTPUT(port_desc)) {
					if (LADSPA_IS_PORT_AUDIO(port_desc)) {
						pvt->ldesc->connect_port(pvt->handle, i, pvt->out_buf);
					} else if (k < MAX_INDEX) {
						pvt->ldesc->connect_port(pvt->handle, i, &pvt->out_ports[k++]);
					}
				}
			}
		}

		break;
	
	case SWITCH_ABC_TYPE_CLOSE:
		{

			if (switch_test_flag((&pvt->fh), SWITCH_FILE_OPEN)) {
				switch_core_file_close(&pvt->fh);
			}

			if (pvt->handle && pvt->ldesc) {
				pvt->ldesc->cleanup(pvt->handle);
			}

			if (pvt->library_handle) {
				unloadLADSPAPluginLibrary(pvt->library_handle);
			}			
		}
		break;

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
	case SWITCH_ABC_TYPE_READ_REPLACE:
		{
			switch_frame_t *rframe;
			int16_t *slin, abuf[SWITCH_RECOMMENDED_BUFFER_SIZE] =  { 0 };
			switch_size_t olen = 0;


			if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
				rframe = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				rframe = switch_core_media_bug_get_write_replace_frame(bug);
			}

			slin = rframe->data;
			
			if (switch_channel_media_ready(channel)) {
				switch_short_to_float(slin, pvt->in_buf, rframe->samples);

				if (switch_test_flag((&pvt->fh), SWITCH_FILE_OPEN)) {
					olen = rframe->samples;
					if (switch_core_file_read(&pvt->fh, abuf, &olen) != SWITCH_STATUS_SUCCESS) {
						switch_codec_implementation_t read_impl = { 0 };
						char *file = switch_core_session_strdup(pvt->session, pvt->fh.file_path);
						switch_core_session_get_read_impl(pvt->session, &read_impl);

						switch_core_file_close(&pvt->fh);
						
						if (switch_core_file_open(&pvt->fh,
												  file,
												  read_impl.number_of_channels,
												  read_impl.actual_samples_per_second, 
												  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_ERROR, "Cannot open file: %s\n", file);
							return SWITCH_FALSE;
						}						

						olen = rframe->samples;
						if (switch_core_file_read(&pvt->fh, abuf, &olen) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_ERROR, "Cannot READ file: %s\n", file);
							return SWITCH_FALSE;
						}
					}
					
					switch_short_to_float(abuf, pvt->file_buf, olen);
				}

				pvt->ldesc->run(pvt->handle, rframe->samples);

				switch_float_to_short(pvt->out_buf, slin, rframe->samples);
			}

			if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
				switch_core_media_bug_set_read_replace_frame(bug, rframe);
			} else {
				switch_core_media_bug_set_write_replace_frame(bug, rframe);
			}

			if (pvt->skip && !--pvt->skip) {
				return SWITCH_FALSE;
			}

		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

switch_status_t stop_ladspa_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((bug = switch_channel_get_private(channel, "ladspa"))) {
		switch_channel_set_private(channel, "ladspa", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t ladspa_session(switch_core_session_t *session, const char *flags, const char *plugin_name, const char *label, const char *params)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_ladspa_t *pvt = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int i, bflags = SMBF_READ_REPLACE | SMBF_ANSWER_REQ;
	char *pstr;
	int argc;
	char *argv[50];
	char *dparams = NULL;

	if (zstr(plugin_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s INVALID PLUGIN\n", switch_channel_get_name(channel));
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(flags)) {
		flags = "r";
	}
	
	if (strchr(flags, 'w')) {
		bflags = SMBF_WRITE_REPLACE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "FLAGS: %s PLUGIN: %s LABEL: %s PARAMS: %s\n", 
					  flags, plugin_name, label, params);

	switch_core_session_get_read_impl(session, &read_impl);

	pvt = switch_core_session_alloc(session, sizeof(*pvt));

   	pvt->session = session;
	if (!zstr(label)) {
		pvt->label_name = switch_core_session_strdup(session, label);
	} else {
		char *p;
		pvt->label_name = switch_core_session_strdup(session, plugin_name);
		if ((p = strrchr(pvt->label_name, '.'))) {
			*p = '\0';
		}
	}

	if (strstr(plugin_name, ".so")) {
		pvt->plugin_name = switch_core_session_strdup(session, plugin_name);
	} else {
		pvt->plugin_name = switch_core_session_sprintf(session, "%s.so", plugin_name);
	}
	
	dparams = switch_core_session_strdup(session, params);

	argc = switch_split(dparams, ' ', argv);

	for (i = 0; i < argc; i++) {
		if (switch_is_number(argv[i])) {
			if (pvt->num_idx < MAX_INDEX) {
				pvt->config[pvt->num_idx] = atof(argv[i]);
				pvt->has_config[pvt->num_idx] = 1;
				pvt->num_idx++;
			}
		} else {
			if (pvt->str_idx < MAX_INDEX) {
				pvt->str_config[pvt->str_idx++] = switch_core_session_strdup(session, argv[i]);
			}
		}
	}

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	pstr = switch_core_session_sprintf(session, "%s|%s|%s|%s", flags, plugin_name, label, params);

	if ((status = switch_core_media_bug_add(session, "ladspa", pstr,
                                            ladspa_callback, pvt, 0, bflags | SMBF_NO_PAUSE, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "ladspa", bug);

	return SWITCH_STATUS_SUCCESS;
}


static void ladspa_parse(switch_core_session_t *session, const char *data)
{
	char *argv[5] = { 0 };
	char *lbuf;

	if (data) {
		lbuf = strdup(data);
		switch_separate_string(lbuf, '|', argv, (sizeof(argv) / sizeof(argv[0])));
	   	ladspa_session(session, argv[0], argv[1], argv[2], argv[3]);
		free(lbuf);
	}
}

#define APP_SYNTAX "<flags>|<plugin>|<label>|<params>"
SWITCH_STANDARD_APP(ladspa_run_function) 
{
	ladspa_parse(session, data);
}



#define API_SYNTAX "<uuid>|<flags>|<plugin>|<label>|<params>"
SWITCH_STANDARD_API(ladspa_api)
{
	char *uuid = NULL;
	char *data;
	char *p;
	switch_core_session_t *ksession = NULL;

	if (!cmd) goto err;

	data = strdup(cmd);

	if ((p = strchr(data, ' '))) {
		uuid = data;
		*p++ = '\0';

		if ((ksession = switch_core_session_locate(uuid))) {
			ladspa_parse(ksession, p);
			switch_core_session_rwunlock(ksession);
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR non-existant UUID\n");
		}
	} else {
		stream->write_function(stream, "-ERR Usage %s\n", API_SYNTAX);
	}

	free(data);

	return SWITCH_STATUS_SUCCESS;

 err:

	stream->write_function(stream, "-ERR Operation Failed\n");

	return SWITCH_STATUS_SUCCESS;
}


/* Macro expands to: switch_status_t mod_ladspa_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_ladspa_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	char *path = getenv("LADSPA_PATH");

	if (zstr(path)) {
		if (switch_directory_exists("/usr/lib64/ladspa/", pool) == SWITCH_STATUS_SUCCESS) {
			setenv("LADSPA_PATH", "/usr/lib64/ladspa/:/usr/local/lib/ladspa", 1);
		} else if (switch_directory_exists("/usr/lib/ladspa/", pool) == SWITCH_STATUS_SUCCESS) {
			setenv("LADSPA_PATH", "/usr/lib/ladspa/:/usr/local/lib/ladspa", 1);
		} else if (switch_directory_exists("/usr/local/lib/ladspa/", pool) == SWITCH_STATUS_SUCCESS) {
			setenv("LADSPA_PATH", "/usr/local/lib/ladspa", 1);
		}
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "ladspa_run", "ladspa_run", NULL, ladspa_run_function, APP_SYNTAX, SAF_NONE);
	SWITCH_ADD_API(api_interface, "uuid_ladspa", "ladspa", ladspa_api, API_SYNTAX);

	switch_console_set_complete("add uuid_ladspa ::console::list_uuid");


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_ladspa_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ladspa_shutdown)
{
	/* Cleanup dynamically allocated config settings */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
