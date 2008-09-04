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
 * Brian West <brian@freeswitch.org>
 * Antonio Gallo <agx@linux.it>
 *
 * mod_fax.c -- Fax Module
 *
 */

#include <switch.h>
#include <spandsp.h>

#define MAX_BLOCK_SIZE 240

SWITCH_MODULE_LOAD_FUNCTION(mod_fax_load);
SWITCH_MODULE_DEFINITION(mod_fax, mod_fax_load, NULL, NULL);

/*
 * output spandsp lowlevel messages
 */
static void span_message(int level, const char *msg)
{
	int fs_log_level = SWITCH_LOG_NOTICE;

	if (msg==NULL) {
        return;
    }

    // TODO: verify all the span_log_levels available
	if (level == SPAN_LOG_ERROR) {
		fs_log_level = SWITCH_LOG_ERROR;
	} else if (level == SPAN_LOG_WARNING) {
		fs_log_level = SWITCH_LOG_WARNING;
	} else {
		fs_log_level = SWITCH_LOG_DEBUG;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, fs_log_level, "%s", msg );
}

/*
 * This function is called when the negotiation is completed
 */

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MARK: Entering phase D\n");
    return T30_ERR_OK;
}

/*
 * This function is called when the fax has finished his job
 */

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    t30_stats_t t;
    const char *local_ident = NULL;
    const char *far_ident = NULL;
    switch_channel_t *chan = (switch_channel_t *) user_data;
    char buf[128];
    
    if (result == T30_ERR_OK) {
        t30_get_transfer_statistics(s, &t);
        far_ident = t30_get_tx_ident(s);
        
        if (!switch_strlen_zero(far_ident)) {
            far_ident = "";
        }
        
        local_ident = t30_get_rx_ident(s);
        
        if (!switch_strlen_zero(local_ident)) {
            local_ident = "";
        }
        
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fax successfully received.\n");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote station id: %s\n", far_ident);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local station id:  %s\n", local_ident);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Pages transferred: %i\n", t.pages_transferred);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image resolution:  %i x %i\n", t.x_resolution, t.y_resolution);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transfer Rate:     %i\n", t.bit_rate);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fax receive not successful - result (%d) %s.\n", result, t30_completion_code_to_str(result));
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
    }

    //TODO: remove the assert once this has been tested
    switch_assert(user_data != NULL);
    //TODO: is the buffer too little?
    switch_channel_set_variable(chan, "FAX_REMOTESTATIONID", far_ident);

    snprintf(buf, sizeof(buf), "%d", t.pages_transferred);
    switch_channel_set_variable(chan, "FAX_PAGES", buf);
    snprintf(buf, sizeof(buf), "%dx%d", t.x_resolution, t.y_resolution);
    switch_channel_set_variable(chan, "FAX_SIZE", buf);
    snprintf(buf, sizeof(buf), "%d", t.bit_rate);
    switch_channel_set_variable(chan, "FAX_SPEED", buf);
    snprintf(buf, sizeof(buf), "%d", result);
    switch_channel_set_variable(chan, "FAX_RESULT", buf);
    snprintf(buf, sizeof(buf), "%s", t30_completion_code_to_str(result));
    switch_channel_set_variable(chan, "FAX_ERROR", buf);
}


/*
 * This function is called whenever a single new page has been received
 */

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    t30_stats_t t;

    if (result) {
        t30_get_transfer_statistics(s, &t);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Pages transferred:  %i\n", t.pages_transferred);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image size:         %i x %i\n", t.width, t.length);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image resolution    %i x %i\n", t.x_resolution, t.y_resolution);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transfer Rate:      %i\n", t.bit_rate);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bad rows            %i\n", t.bad_rows);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Longest bad row run %i\n", t.longest_bad_row_run);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Compression type    %i %s\n", t.encoding, t4_encoding_to_str(t.encoding));
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image size (bytes)  %i\n", t.image_size);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
    }
    return T30_ERR_OK;
}

SWITCH_STANDARD_APP(rxfax_function)
{
    switch_channel_t *channel;
    switch_codec_t *orig_read_codec = NULL;
    switch_codec_t read_codec = {0};
    switch_codec_t write_codec = {0};
    switch_frame_t *read_frame = {0};
    switch_frame_t write_frame = {0};
    switch_status_t status;
    fax_state_t fax;
    int16_t buf[512];
    int tx = 0;
    int calling_party = FALSE;
    /* Channels variable parsing */
    char *file_name = NULL;
    const char *fax_local_debug = NULL;
    int debug = FALSE;
    const char *fax_local_number = NULL;
    const char *fax_local_name = NULL;
    const char *fax_local_subname = NULL;
    const char *fax_local_ecm = NULL;
    const char *fax_local_v17 = NULL;

    // make sure we have a valid channel when starting the FAX application
    channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);	

	/* reset output variables */
    switch_channel_set_variable(channel, "FAX_REMOTESTATIONID", "unknown");
    switch_channel_set_variable(channel, "FAX_PAGES",   "0");
    switch_channel_set_variable(channel, "FAX_SIZE",    "0");
    switch_channel_set_variable(channel, "FAX_SPEED",   "0");
    switch_channel_set_variable(channel, "FAX_RESULT",  "1");
    switch_channel_set_variable(channel, "FAX_ERROR",   "fax not received yet");

    // Answer the call, otherwise we're not getting incoming audio
	switch_channel_answer(channel);

    // TODO: could be a good idea to disable ECHOCAN on ZAP channels and to reset volumes too

    /* We store the original channel codec before switching both
     * legs of the calls to a linear 16 bit codec that is the one
     * used internally by spandsp and FS will do the transcoding
     * from G.711 or any other original codec
     */
    orig_read_codec = switch_core_session_get_read_codec(session);

    if (switch_core_codec_init(&read_codec, 
                               "L16", 
                               NULL, 
                               orig_read_codec->implementation->samples_per_second, 
                               orig_read_codec->implementation->microseconds_per_frame / 1000, 
                               1, 
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
                               NULL, 
                               switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16\n");
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16");
        goto done;
    }

    if (switch_core_codec_init(&write_codec, 
                               "L16", 
                               NULL, 
                               orig_read_codec->implementation->samples_per_second, 
                               orig_read_codec->implementation->microseconds_per_frame / 1000, 
                               1, 
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
                               NULL, 
                               switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16\n");
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16");
        goto done;
    }

    /* SpanDSP initialization */
    //TODO: check spandsp code to see if we need to clear the fax structure
    memset(&fax, 0, sizeof(fax));
    fax_init(&fax, calling_party);
    //TODO: fax_init return NULL in case of failed initialization

    //TODO: did i ported this from app_TXfax.c and it has no use there?
    fax_set_transmit_on_idle(&fax, TRUE);

    //TODO: set spanlog debug to be outputted somewhere 
    span_log_set_message_handler(&fax.logging, span_message);
    span_log_set_message_handler(&fax.t30.logging, span_message);

    /*
     * Enable options based on channel variables and input parameters
     */

    /* file_name - Sets the TIFF filename where do you want to save the fax */
    file_name = switch_core_session_strdup(session, data);
    //TODO: check file_name is not NULL ?
    t30_set_rx_file(&fax.t30, file_name, -1);

    /* FAX_DEBUG - enable extra debugging if defined */
    fax_local_debug = switch_channel_get_variable(channel, "FAX_DEBUG");
    debug = (fax_local_debug == NULL) ? FALSE : TRUE;
    if (debug) {
        span_log_set_level(&fax.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
        span_log_set_level(&fax.t30.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    }

    /* FAX_LOCAL_NUMBER - Set the receiving station phone number */
    fax_local_number = switch_channel_get_variable(channel, "FAX_LOCAL_NUMBER");
    if (fax_local_number==NULL) {
        fax_local_number="";
    }
    t30_set_tx_ident(&fax.t30, fax_local_number);

    /* FAX_LOCAL_NAME - Set the receiving station ID name (string) */
    fax_local_name = switch_channel_get_variable(channel, "FAX_LOCAL_NAME");
    if (fax_local_name==NULL) {
        fax_local_name="";
    }
    t30_set_tx_page_header_info(&fax.t30, fax_local_name);

    /* FAX_LOCAL_SUBNAME - Set the receiving station ID sub name */
    fax_local_subname = switch_channel_get_variable(channel, "FAX_LOCAL_SUBNAME");
    if (fax_local_subname==NULL) {
        fax_local_subname="";
    }
	t30_set_tx_sub_address(&fax.t30, fax_local_subname);

    /* FAX_DISABLE_ECM - Set if you want ECM on or OFF */
    fax_local_ecm = switch_channel_get_variable(channel, "FAX_DISABLE_ECM");
    if (fax_local_ecm == NULL ) {
        t30_set_ecm_capability(&fax.t30, TRUE);
		t30_set_supported_compressions(&fax.t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
    } else {
        t30_set_ecm_capability(&fax.t30, FALSE);
	}

    /* FAX_DISABLE_V17 - set if you want 9600 or V17 (14.400) */
    fax_local_v17 = switch_channel_get_variable(channel, "FAX_DISABLE_V17");
    if (fax_local_v17 == NULL) {
		t30_set_supported_modems(&fax.t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER | T30_SUPPORT_V17 );
    } else {
		t30_set_supported_modems(&fax.t30, T30_SUPPORT_V29 | T30_SUPPORT_V27TER);
    }

	/* Support for different image sizes && resolutions */
	t30_set_supported_image_sizes(&fax.t30, T30_SUPPORT_US_LETTER_LENGTH | T30_SUPPORT_US_LEGAL_LENGTH | T30_SUPPORT_UNLIMITED_LENGTH
			| T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
	t30_set_supported_resolutions(&fax.t30, T30_SUPPORT_STANDARD_RESOLUTION | T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION
			| T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);

    /* set phase handlers callbaks */
	t30_set_phase_b_handler(&fax.t30, phase_b_handler, NULL);
    t30_set_phase_d_handler(&fax.t30, phase_d_handler, NULL);
    t30_set_phase_e_handler(&fax.t30, phase_e_handler, channel);

    // TODO: ?? what this does ??
    write_frame.codec = &write_codec;
    write_frame.data = buf;

    /*
     * now we enter a loop where we read audio frames to the channels and will pass it to spandsp
     * and if there is some outgoing frame we'll send it back to the calling fax machine
     */
    while(switch_channel_ready(channel)) {

        // read new audio frame from the channel
        status = switch_core_session_read_frame(session, &read_frame, -1, 0);
        if (!SWITCH_READ_ACCEPTABLE(status)) {
            goto done;
        }

        // pass the new incoming audio frame to the fax_rx application
        fax_rx(&fax, (int16_t *)read_frame->data, read_frame->samples);
        if ((tx = fax_tx(&fax, (int16_t *) &buf, write_codec.implementation->samples_per_frame)) < 0) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fax Error\n");
            goto done;
        }
        
        if (tx) {
            write_frame.datalen = tx * sizeof(int16_t);
            write_frame.samples = tx;
        
            if (switch_core_session_write_frame(session, &write_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad Write\n");
                goto done;
            }
        }
    }

 done:

    // shutdown spandsp so it can create our tiff
    t30_terminate(&fax.t30);
    fax_release(&fax);
    
    // restore the original codecs over the channels
    if (read_codec.implementation) {
        switch_core_codec_destroy(&read_codec);
    }

    if (write_codec.implementation) {
        switch_core_codec_destroy(&write_codec);
    }

    if (orig_read_codec) {
        switch_core_session_set_read_codec(session, orig_read_codec);
    }

}

SWITCH_MODULE_LOAD_FUNCTION(mod_fax_load)
{
    switch_application_interface_t *app_interface;
    
	/* connect my internal structure to the blank pointer passed to me */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    
    SWITCH_ADD_APP(app_interface, "rxfax", "Trivial FAX Receive Application", "Trivial FAX Receive Application", rxfax_function, "", SAF_NONE);

    /* TODO: its important to debug the exact spandsp used
	"RxFax using spandsp %i %i\n", SPANDSP_RELEASE_DATE, SPANDSP_RELEASE_TIME 
    */
    
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
