/* 
 * mod_fax.c (unholy forbidden interface to spandsp)
 *
 * Brian West <brian.west@mac.com>
 *
 *
 * mod_fax.c -- Fax Module
 *
 */

#include <switch.h>
#include <spandsp.h>

#define MAX_BLOCK_SIZE 240

SWITCH_MODULE_LOAD_FUNCTION(mod_fax_load);
SWITCH_MODULE_DEFINITION(mod_fax, mod_fax_load, NULL, NULL);

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    t30_stats_t t;
    char local_ident[21];
    char far_ident[21];
    
    if (result == T30_ERR_OK)
        {
            t30_get_transfer_statistics(s, &t);
            t30_get_far_ident(s, far_ident);
            t30_get_local_ident(s, local_ident);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fax successfully received.\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote station id: %s\n", far_ident);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local station id:  %s\n", local_ident);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Pages transferred: %i\n", t.pages_transferred);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image resolution:  %i x %i\n", t.x_resolution, t.y_resolution);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transfer Rate:     %i\n", t.bit_rate);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        }
    else
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fax receive not successful - result (%d) %s.\n", result, t30_completion_code_to_str(result));
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        }
}

static void phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    t30_stats_t t;

    if (result)
        {
            t30_get_transfer_statistics(s, &t);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Pages transferred:  %i\n", t.pages_transferred);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image size:         %i x %i\n", t.width, t.length);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image resolution    %i x %i\n", t.x_resolution, t.y_resolution);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Transfer Rate:      %i\n", t.bit_rate);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bad rows            %i\n", t.bad_rows);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Longest bad row run %i\n", t.longest_bad_row_run);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Compression type    %i\n", t.encoding);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Image size (bytes)  %i\n", t.image_size);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "==============================================================================\n");
        }
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
    char *file_name = NULL;

    file_name = switch_core_session_strdup(session, data);

    channel = switch_core_session_get_channel(session);
	assert(channel != NULL);	

	switch_channel_answer(channel);

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

    fax_init(&fax, calling_party);
    t30_set_local_ident(&fax.t30_state, "test");
    t30_set_header_info(&fax.t30_state, "test");
    t30_set_rx_file(&fax.t30_state, file_name, -1);

    t30_set_phase_d_handler(&fax.t30_state, phase_d_handler, NULL);
    t30_set_phase_e_handler(&fax.t30_state, phase_e_handler, NULL);

    t30_set_ecm_capability(&fax.t30_state, TRUE);
    t30_set_supported_compressions(&fax.t30_state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

    t30_set_supported_image_sizes(&fax.t30_state, T30_SUPPORT_US_LETTER_LENGTH | T30_SUPPORT_US_LEGAL_LENGTH | T30_SUPPORT_UNLIMITED_LENGTH
                                  | T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
    t30_set_supported_resolutions(&fax.t30_state, T30_SUPPORT_STANDARD_RESOLUTION | T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION
                                  | T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);

    write_frame.codec = &write_codec;
    write_frame.data = buf;

    while(switch_channel_ready(channel)) {
        status = switch_core_session_read_frame(session, &read_frame, -1, 0);
        if (!SWITCH_READ_ACCEPTABLE(status)) {
            goto done;
        }

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

    t30_terminate(&fax.t30_state);
    fax_release(&fax);
    
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
