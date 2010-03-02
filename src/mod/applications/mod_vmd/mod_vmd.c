/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2008, Eric des Courtis <eric.des.courtis@benbria.com>
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
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Copyright (C) Benbria. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Eric des Courtis <eric.des.courtis@benbria.com>
 *
 * Special thanks to the following companies for their help:
 * 	- JohnnyVoIP
 * 	- Magor Communications Corporation
 *
 * Special thanks to the following people for their help:
 * 	- The FreeSWITCH Team
 * 	- Matt Battig
 * 	- Dean Swan
 * 	- Lucas Cornelisse
 * 	- Kevin Green
 *
 * mod_vmd.c -- Voicemail Detection Module
 *
 * This module detects voicemail beeps at any frequency in O(1) time.
 * 
 */

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WIN32
#include <float.h>
#define ISNAN(x) (!!(_isnan(x)))
#else
#define ISNAN(x) (isnan(x))
#endif

/*! Number of points for beep detection. */
#define POINTS 32

/*! Number of valid points required for beep detection. */
#define VALID 22

/*! Maximum number of invalid points to declare beep has stopped. */
#define MAX_CHIRP 22

/*! Minimum time for a beep. */
#define MIN_TIME 8000

/*! Minimum amplitude of the signal. */
#define MIN_AMPL 0.10

/*! Minimum beep frequency. */
#define MIN_FREQ (600)

/*! Maximum beep frequency. */
#define MAX_FREQ (1100)

/*! \brief Helper for amplitude calculation
 *
 *  The function is defined as \f$\psi{(x)} = {x^2_1} - {x_2} {x_0}\f$
 *
 *  @author Eric des Courtis
 *  @param x An array of 3 or more samples.
 *  @return The value of \f$\psi{(x)}\f$.
 */
#define PSI(x) (x[1]*x[1]-x[2]*x[0])

/*! Sample rate NOTE: this should be dynamic in the future. */
#define F (8000)

/*! \brief Conversion of frequency to Hz
 *
 * \f$F = \frac{f}{{2}{\pi}}\f$ 
 */
#define TO_HZ(f) ((F * f) / (2.0 * M_PI))

/* Number of points in discreet energy separation. */
#define P (5)

/* Maximum signed value of int16_t 
 * DEPRECATED */
#define ADJUST (32768)
/* Same as above times two 
 * DEPRECATED */
#define ADJUST_MAX (65536)

/*! Discreet energy separation tolerance to error. */
#define TOLERANCE (0.20)

/*! Maximum value within tolerance. */
#define TOLERANCE_T(m) (m + (m * TOLERANCE))

/*! Minimum value within tolerance. */
#define TOLERANCE_B(m) (m - (m * TOLERANCE))

/*! Syntax of the API call. */
#define VMD_SYNTAX "<uuid> <command>"

/*! Number of expected parameters in api call. */
#define VMD_PARAMS 2

/*! FreeSWITCH CUSTOM event type. */
#define VMD_EVENT_BEEP "vmd::beep"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vmd_shutdown);
SWITCH_STANDARD_API(vmd_api_main);

SWITCH_MODULE_LOAD_FUNCTION(mod_vmd_load);
SWITCH_MODULE_DEFINITION(mod_vmd, mod_vmd_load, NULL, NULL);
SWITCH_STANDARD_APP(vmd_start_function);

/*! Type that holds state information about the beep. */
typedef enum vmd_state {
	BEEP_DETECTED, BEEP_NOT_DETECTED
} vmd_state_t;

/*! Type that holds data for 5 points of discreet energy separation */
typedef struct vmd_point {
	double freq;
	double ampl;
} vmd_point_t;

/*! Type that holds codec information. */
typedef struct vmd_codec_info {
	/*! The sampling rate of the audio stream. */
	int rate;
	/*! The number of channels. */
	int channels;
} vmd_codec_info_t;

/*! Type that holds session information pertinent to the vmd module. */
typedef struct vmd_session_info {
	/*! State of the session. */
	vmd_state_t state;
	/*! Snapshot of DESA samples. */
	vmd_point_t points[POINTS];
	/*! Internal FreeSWITCH session. */
	switch_core_session_t *session;
	/*! Codec information for the session. */
	vmd_codec_info_t vmd_codec;
	/*! Current position in the snapshot. */
	unsigned int pos;
	/*! Frequency aproximation of a detected beep. */
	double beep_freq;
	/*! A count of how long a distinct beep was detected
	 *  by the discreet energy separation algorithm. */
	switch_size_t timestamp;
	/*! The MIN_TIME to use for this call */
	int minTime;
} vmd_session_info_t;

static switch_bool_t process_data(vmd_session_info_t *vmd_info, switch_frame_t *frame);
static switch_bool_t vmd_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type);
static double freq_estimator(double *x);
static double ampl_estimator(double *x);
static void convert_pts(int16_t *i_pts, double *d_pts, int16_t max);
static void find_beep(vmd_session_info_t *vmd_info, switch_frame_t *frame);
static double median(double *m, int n);

/*
#define PRINT(a) do{ switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, a); }while(0)
#define PRINT2(a, b) do{ switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, a, b); }while(0)
*/

/*! \brief The callback function that is called when new audio data becomes available 
 *
 * @author Eric des Courtis
 * @param bug A reference to the media bug.
 * @param user_data The session information for this call.
 * @param type The switch callback type.
 * @return The success or failure of the function.
 */
static switch_bool_t vmd_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	vmd_session_info_t *vmd_info;
	switch_codec_t *read_codec;
	switch_frame_t *frame;

	vmd_info = (vmd_session_info_t *) user_data;
	if (vmd_info == NULL) {
		return SWITCH_FALSE;
	}

	switch (type) {

	case SWITCH_ABC_TYPE_INIT:
		read_codec = switch_core_session_get_read_codec(vmd_info->session);
		vmd_info->vmd_codec.rate = read_codec->implementation->samples_per_second;
		vmd_info->vmd_codec.channels = read_codec->implementation->number_of_channels;
		break;

	case SWITCH_ABC_TYPE_READ_PING:
	case SWITCH_ABC_TYPE_CLOSE:
	case SWITCH_ABC_TYPE_READ:
	case SWITCH_ABC_TYPE_WRITE:
		break;

	case SWITCH_ABC_TYPE_READ_REPLACE:
		frame = switch_core_media_bug_get_read_replace_frame(bug);
		return process_data(vmd_info, frame);

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		break;
	}

	return SWITCH_TRUE;
}

/*! \brief Process and convert data to be used by the find_beep() function 
 *
 * @author Eric des Courtis
 * @param vmd_info The session information associated with the call.
 * @param frame The audio data.
 * @return The success or failure of the function.
 */
static switch_bool_t process_data(vmd_session_info_t *vmd_info, switch_frame_t *frame)
{
	uint32_t i;
	unsigned int j;
	double pts[P];
	int16_t *data;
	int16_t max;
	switch_ssize_t len;

	len = frame->samples * sizeof(int16_t);
	data = (int16_t *) frame->data;

	for (max = (int16_t) abs(data[0]), i = 1; i < frame->samples; i++) {
		if ((int16_t) abs(data[i]) > max) {
			max = (int16_t) abs(data[i]);
		}
	}

/*
    if (vmd_info->data_len != len){
	vmd_info->data_len = len;
	if (vmd_info->data != NULL) free(vmd_info->data);
	vmd_info->data = (int16_t *)malloc(len);
	if (vmd_info->data == NULL) return SWITCH_FALSE;
    } 

    (void)memcpy(vmd_info->data, data, len);
    for(i = 2; i < frame->samples; i++){
	vmd_info->data[i] = 
	    0.0947997 * data[i] 
	    - 
	    0.0947997 * data[i - 2] 
	    - 
	    1.4083405 * vmd_info->data[i - 1] 
	    + 
	    0.8104005 * vmd_info->data[i - 2];
    }
*/

	for (i = 0, j = vmd_info->pos; i < frame->samples; j++, j %= POINTS, i += 5) {
		/*      convert_pts(vmd_info->data + i, pts); */
		convert_pts(data + i, pts, max);
		vmd_info->points[j].freq = TO_HZ(freq_estimator(pts));
		vmd_info->points[j].ampl = ampl_estimator(pts);
		vmd_info->pos = j % POINTS;
		find_beep(vmd_info, frame);
	}

	return SWITCH_TRUE;
}

/*! \brief Find voicemail beep in the audio stream 
 *
 * @author Eric des Courtis
 * @param vmd_info The session information associated with the call.
 * @param frame The audio data.
 * @return The success or failure of the function.
 */
static void find_beep(vmd_session_info_t *vmd_info, switch_frame_t *frame)
{
	int i;
	int c;
	double m[POINTS];
	double med;
	unsigned int j = (vmd_info->pos + 1) % POINTS;
	unsigned int k = j;
	switch_event_t *event;
	switch_status_t status;
	switch_event_t *event_copy;
	switch_channel_t *channel = switch_core_session_get_channel(vmd_info->session);

	switch (vmd_info->state) {
	case BEEP_DETECTED:
		for (c = 0, i = 0; i < POINTS; j++, j %= POINTS, i++) {
			vmd_info->timestamp++;
			if (vmd_info->points[j].freq < TOLERANCE_T(vmd_info->beep_freq) && vmd_info->points[j].freq > TOLERANCE_B(vmd_info->beep_freq)) {
				c++;
				vmd_info->beep_freq = (vmd_info->beep_freq * 0.95) + (vmd_info->points[j].freq * 0.05);
			}
		}

		if (c < (POINTS - MAX_CHIRP)) {
			vmd_info->state = BEEP_NOT_DETECTED;
			if (vmd_info->timestamp < (switch_size_t) vmd_info->minTime) {
				break;
			}

			status = switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VMD_EVENT_BEEP);
			if (status != SWITCH_STATUS_SUCCESS) {
				return;
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Beep-Status", "stop");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Beep-Time", "%d", (int) vmd_info->timestamp / POINTS);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(vmd_info->session));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Frequency", "%6.4lf", vmd_info->beep_freq);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-command", "vmd");

			if ((switch_event_dup(&event_copy, event)) != SWITCH_STATUS_SUCCESS) {
				return;
			}

			switch_core_session_queue_event(vmd_info->session, &event);
			switch_event_fire(&event_copy);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(vmd_info->session), SWITCH_LOG_INFO, "<<< VMD - Beep Detected >>>\n");
			switch_channel_set_variable(channel, "vmd_detect", "TRUE");

			vmd_info->timestamp = 0;
		}

		break;

	case BEEP_NOT_DETECTED:

		for (i = 0; i < POINTS; k++, k %= POINTS, i++) {
			m[i] = vmd_info->points[k].freq;
			if (ISNAN(m[i])) {
				m[i] = 0.0;
			}
		}

		med = median(m, POINTS);
		if (ISNAN(med)) {
			for (i = 0; i < POINTS; i++) {
				if (!ISNAN(m[i])) {
					med = m[i];
					break;
				}
			}
		}

		for (c = 0, i = 0; i < POINTS; j++, j %= POINTS, i++) {
			if (vmd_info->points[j].freq < TOLERANCE_T(med) && vmd_info->points[j].freq > TOLERANCE_B(med)) {
				if (vmd_info->points[j].ampl > MIN_AMPL && vmd_info->points[j].freq > MIN_FREQ && vmd_info->points[j].freq < MAX_FREQ) {
					c++;
				}
			}
		}

		if (c >= VALID) {
			vmd_info->state = BEEP_DETECTED;
			vmd_info->beep_freq = med;
			vmd_info->timestamp = 0;
		}

		break;
	}
}

/*! \brief Find the median of an array of doubles 
 *
 * @param m Array of frequency samples.
 * @param n Number of samples in the array.
 * @return The median.
 */
static double median(double *m, int n)
{
	int i;
	int less;
	int greater;
	int equal;
	double min;
	double max;
	double guess;
	double maxltguess;
	double mingtguess;

	min = max = m[0];

	for (i = 1; i < n; i++) {
		if (m[i] < min)
			min = m[i];
		if (m[i] > max)
			max = m[i];
	}

	for (;;) {
		guess = (min + max) / 2;
		less = 0;
		greater = 0;
		equal = 0;
		maxltguess = min;
		mingtguess = max;

		for (i = 0; i < n; i++) {
			if (m[i] < guess) {
				less++;
				if (m[i] > maxltguess) {
					maxltguess = m[i];
				}
			} else if (m[i] > guess) {
				greater++;
				if (m[i] < mingtguess) {
					mingtguess = m[i];
				}
			} else {
				equal++;
			}
		}

		if (less <= (n + 1) / 2 && greater <= (n + 1) / 2) {
			break;
		} else if (less > greater) {
			max = maxltguess;
		} else {
			min = mingtguess;
		}
	}

	if (less >= (n + 1) / 2) {
		return maxltguess;
	} else if (less + equal >= (n + 1) / 2) {
		return guess;
	}

	return mingtguess;
}

/*! \brief Convert many points for Signed L16 to relative floating point 
 *
 * @author Eric des Courtis
 * @param i_pts Array of P 16 bit integer audio samples.
 * @param d_pts Array of P double floating point audio samples.
 * @param max The maximum value in the entire audio frame.
 * @return Nothing.
 */
static void convert_pts(int16_t *i_pts, double *d_pts, int16_t max)
{
	int i;
	for (i = 0; i < P; i++) {
		/*! Signed L16 to relative floating point conversion */
		d_pts[i] = ((((double) (i_pts[i]) + (double) max) / (double) (2 * max)) - 0.5) * 2.0;
	}
}

/*! \brief Amplitude estimator for DESA-2
 *  
 *  The function is defined as \f$A = \sqrt{\frac{\psi{(x)}}{\sin{\Omega^2}}}\f$
 *  
 *  @author Eric des Courtis
 *  @param x An array of 5 evenly spaced audio samples \f$x_0, x_1, x_2, x_3, x_4\f$.
 *  @return The estimated amplitude.
 */
double ampl_estimator(double *x)
{
	double freq_sq;

	freq_sq = freq_estimator(x);
	freq_sq *= freq_sq;

	return sqrt(PSI(x) / sin(freq_sq));
}

/*! \brief The DESA-2 algorithm 
 *
 *  The function is defined as \f$f = \frac{1}{2}\arccos{\frac{{{x^2_2} - 
 *  {x_0}{x_4}} - {{x^2_1} - 
 *  {x_0}{x_2}} - {{x^2_3} - 
 *  {x_2}{x_4}}}
 *  {{2}({x^2_2} - {x_1}{x_3})}}\f$
 *
 * @author Eric des Courtis
 * @param x An array for 5 evenly spaced audio samples \f$x_0, x_1, x_2, x_3, x_4\f$.
 * @return A frequency estimate.
 */
double freq_estimator(double *x)
{
	return 0.5 * acos((((x[2] * x[2]) - (x[0] * x[4]))
					   - ((x[1] * x[1]) - (x[0] * x[2]))
					   - ((x[3] * x[3]) - (x[2] * x[4])))
					  / (2.0 * ((x[2] * x[2]) - (x[1] * x[3])))

		);
}

/*! \brief FreeSWITCH module loading function 
 *
 * @author Eric des Courtis
 * @return Load success or failure.
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_vmd_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Voicemail detection enabled\n");

	SWITCH_ADD_APP(app_interface, "vmd", "Detect beeps", "Detect voicemail beeps", vmd_start_function, "[start] [stop]", SAF_NONE);

	SWITCH_ADD_API(api_interface, "vmd", "Detected voicemail beeps", vmd_api_main, VMD_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH application handler function.
 *  This handles calls made from applications such as LUA and the dialplan
 *
 * @author Eric des Courtis
 * @return Success or failure of the function.
 */
SWITCH_STANDARD_APP(vmd_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel;
	vmd_session_info_t *vmd_info;
	int i;
	const char *minTimeString;
	int mintime = 0;

	if (session == NULL)
		return;

	channel = switch_core_session_get_channel(session);

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_vmd_");
	/* If yes */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(data, "stop") == 0) {
			switch_channel_set_private(channel, "_vmd_", NULL);
			switch_core_media_bug_remove(session, &bug);
			return;
		}

		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");

		return;
	}

	vmd_info = (vmd_session_info_t *) switch_core_session_alloc(session, sizeof(vmd_session_info_t)
		);

	vmd_info->state = BEEP_NOT_DETECTED;
	vmd_info->session = session;
	vmd_info->pos = 0;
	/*
	   vmd_info->data = NULL;
	   vmd_info->data_len = 0;
	 */
	for (i = 0; i < POINTS; i++) {
		vmd_info->points[i].freq = 0.0;
		vmd_info->points[i].ampl = 0.0;
	}

	status = switch_core_media_bug_add(session, "vmd", NULL, vmd_callback, vmd_info, 0, SMBF_READ_REPLACE, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure hooking to stream\n");
		return;
	}

	switch_channel_set_private(channel, "_vmd_", bug);

	if ((minTimeString = switch_channel_get_variable(channel, "vmd_min_time")) && (mintime = atoi(minTimeString))) {
		vmd_info->minTime = mintime;
	} else {
		vmd_info->minTime = MIN_TIME;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MIN_TIME for call: %d\n", vmd_info->minTime);
}

/*! \brief Called when the module shuts down
 *
 * @author Eric des Courtis
 * @return The success or failure of the function.
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vmd_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Voicemail detection disabled\n");

	return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH API handler function.
 *  This function handles API calls such as the ones from mod_event_socket and in some cases
 *  scripts such as LUA scripts.
 *
 *  @author Eric des Courtis
 *  @return The success or failure of the function.
 */
SWITCH_STANDARD_API(vmd_api_main)
{
	switch_core_session_t *vmd_session = NULL;
	switch_media_bug_t *bug;
	vmd_session_info_t *vmd_info;
	switch_channel_t *channel;
	switch_status_t status;
	int argc;
	char *argv[VMD_PARAMS];
	char *ccmd = NULL;
	char *uuid;
	char *command;
	int i;

	/* No command? Display usage */
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", VMD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	/* Duplicated contents of original string */
	ccmd = strdup(cmd);
	/* Separate the arguments */
	argc = switch_separate_string(ccmd, ' ', argv, VMD_PARAMS);

	/* If we don't have the expected number of parameters 
	 * display usage */
	if (argc != VMD_PARAMS) {
		stream->write_function(stream, "-USAGE: %s\n", VMD_SYNTAX);
		goto end;
	}

	uuid = argv[0];
	command = argv[1];

	/* using uuid locate a reference to the FreeSWITCH session */
	vmd_session = switch_core_session_locate(uuid);

	/* If the session was not found exit */
	if (vmd_session == NULL) {
		stream->write_function(stream, "-USAGE: %s\n", VMD_SYNTAX);
		goto end;
	}

	/* Get current channel of the session to tag the session
	 * This indicates that our module is present */
	channel = switch_core_session_get_channel(vmd_session);

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_vmd_");
	/* If yes */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(command, "stop") == 0) {
			switch_channel_set_private(channel, "_vmd_", NULL);
			switch_core_media_bug_remove(vmd_session, &bug);
			switch_safe_free(ccmd);
			stream->write_function(stream, "+OK\n");
			goto end;
		}

		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		goto end;
	}

	/* If we don't see the expected start exit */
	if (strcasecmp(command, "start") != 0) {
		stream->write_function(stream, "-USAGE: %s\n", VMD_SYNTAX);
		goto end;
	}

	/* Allocate memory attached to this FreeSWITCH session for
	 * use in the callback routine and to store state information */
	vmd_info = (vmd_session_info_t *) switch_core_session_alloc(vmd_session, sizeof(vmd_session_info_t)
		);

	/* Set initial values and states */
	vmd_info->state = BEEP_NOT_DETECTED;
	vmd_info->session = vmd_session;
	vmd_info->pos = 0;
/*
    vmd_info->data = NULL;
    vmd_info->data_len = 0;
*/

	for (i = 0; i < POINTS; i++) {
		vmd_info->points[i].freq = 0.0;
		vmd_info->points[i].ampl = 0.0;
	}

	/* Add a media bug that allows me to intercept the 
	 * reading leg of the audio stream */
	status = switch_core_media_bug_add(vmd_session, "vmd", NULL, vmd_callback, vmd_info, 0, SMBF_READ_REPLACE, &bug);

	/* If adding a media bug fails exit */
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure hooking to stream\n");
		goto end;
	}

	/* Set the vmd tag to detect an existing vmd media bug */
	switch_channel_set_private(channel, "_vmd_", bug);

	/* Everything went according to plan! Notify the user */
	stream->write_function(stream, "+OK\n");


  end:

	if (vmd_session) {
		switch_core_session_rwunlock(vmd_session);
	}

	switch_safe_free(ccmd);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
