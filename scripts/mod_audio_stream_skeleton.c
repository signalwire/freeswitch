/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * mod_audio_stream - WebSocket Audio Streaming Module for Brain-Core Integration
 *
 * This module streams audio from FreeSWITCH to Brain-Core Orchestrator via WebSocket
 *
 * Features:
 * - Real-time audio streaming over WebSocket
 * - Bi-directional audio flow (send & receive)
 * - Low-latency audio chunks (20ms frames)
 * - Call state synchronization
 *
 * Architecture:
 * Phone → FreeSWITCH (RTP) → mod_audio_stream (WebSocket) → Brain-Core → AI Services
 */

#include <switch.h>
#include <libwebsockets.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_audio_stream_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_audio_stream_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_audio_stream_runtime);
SWITCH_MODULE_DEFINITION(mod_audio_stream, mod_audio_stream_load, mod_audio_stream_shutdown, mod_audio_stream_runtime);

static struct {
    char *brain_core_ws_url;          // Brain-Core WebSocket URL (e.g., ws://localhost:3000)
    int audio_chunk_ms;                // Audio chunk size in milliseconds (default: 20ms)
    int sample_rate;                   // Audio sample rate (default: 8000 or 16000)
    int channels;                      // Audio channels (default: 1 - mono)
    switch_memory_pool_t *pool;
    struct lws_context *ws_context;
} globals;

/* WebSocket connection data per call */
typedef struct {
    struct lws *wsi;                   // WebSocket instance
    switch_core_session_t *session;    // FreeSWITCH session
    char call_id[128];                 // Unique call identifier
    switch_queue_t *audio_queue;       // Queue for audio chunks
    switch_mutex_t *mutex;
    switch_bool_t active;
} audio_stream_context_t;

/* WebSocket callback handler */
static int callback_audio_stream(struct lws *wsi, enum lws_callback_reasons reason,
                                  void *user, void *in, size_t len)
{
    audio_stream_context_t *context = (audio_stream_context_t *)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                            "WebSocket connected to Brain-Core\n");
            if (context) {
                context->active = SWITCH_TRUE;
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            /* Received audio from Brain-Core (TTS response) */
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
                            "Received %zu bytes from Brain-Core\n", len);
            /* TODO: Queue audio for playback to caller */
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            /* Ready to send audio to Brain-Core */
            /* TODO: Dequeue audio chunks and send via lws_write() */
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                            "WebSocket disconnected from Brain-Core\n");
            if (context) {
                context->active = SWITCH_FALSE;
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                            "WebSocket connection error to Brain-Core\n");
            if (context) {
                context->active = SWITCH_FALSE;
            }
            break;

        default:
            break;
    }

    return 0;
}

/* WebSocket protocols */
static struct lws_protocols protocols[] = {
    {
        "audio-stream-protocol",
        callback_audio_stream,
        sizeof(audio_stream_context_t),
        4096, /* rx buffer size */
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

/* Application: stream_to_brain - Start audio streaming to Brain-Core */
SWITCH_STANDARD_APP(stream_to_brain_function)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    audio_stream_context_t *context = NULL;
    switch_codec_t codec;
    switch_frame_t *read_frame;
    switch_status_t status;

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
                     "Starting audio stream to Brain-Core\n");

    /* Allocate context */
    context = switch_core_session_alloc(session, sizeof(audio_stream_context_t));
    context->session = session;
    switch_snprintf(context->call_id, sizeof(context->call_id), "%s",
                   switch_core_session_get_uuid(session));

    /* Initialize WebSocket connection */
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = globals.ws_context;
    ccinfo.address = "localhost";  // TODO: Parse from brain_core_ws_url
    ccinfo.port = 3000;
    ccinfo.path = "/audio-stream";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name;
    ccinfo.userdata = context;

    context->wsi = lws_client_connect_via_info(&ccinfo);
    if (!context->wsi) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
                         "Failed to connect to Brain-Core WebSocket\n");
        return;
    }

    /* Set up audio codec (L16 @ 16kHz mono) */
    if (switch_core_codec_init(&codec,
                               "L16",
                               NULL,
                               NULL,
                               16000,
                               20, /* 20ms ptime */
                               1, /* mono */
                               SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
                               NULL,
                               switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
                         "Failed to initialize codec\n");
        return;
    }

    switch_core_session_set_read_codec(session, &codec);

    /* Main audio streaming loop */
    while (switch_channel_ready(channel) && context->active) {
        status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

        if (!SWITCH_READ_ACCEPTABLE(status)) {
            break;
        }

        if (switch_test_flag(read_frame, SFF_CNG)) {
            /* Comfort Noise Generation - skip */
            continue;
        }

        /* Send audio frame to Brain-Core via WebSocket */
        /* TODO: Queue frame and trigger LWS_CALLBACK_CLIENT_WRITEABLE */
        lws_callback_on_writable(context->wsi);
        lws_service(globals.ws_context, 0);
    }

    /* Cleanup */
    switch_core_codec_destroy(&codec);

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
                     "Audio stream to Brain-Core ended\n");
}

/* API: audio_stream_status - Check connection status */
SWITCH_STANDARD_API(audio_stream_status_api)
{
    stream->write_function(stream, "+OK Brain-Core WebSocket: %s\n",
                          globals.ws_context ? "Active" : "Inactive");
    return SWITCH_STATUS_SUCCESS;
}

/* Configuration loader */
static switch_status_t load_config(void)
{
    switch_xml_t cfg, xml, settings, param;

    if (!(xml = switch_xml_open_cfg("audio_stream.conf", &cfg, NULL))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                         "Failed to open audio_stream.conf\n");
        return SWITCH_STATUS_FALSE;
    }

    if ((settings = switch_xml_child(cfg, "settings"))) {
        for (param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *)switch_xml_attr_soft(param, "name");
            char *val = (char *)switch_xml_attr_soft(param, "value");

            if (!strcmp(var, "brain-core-ws-url")) {
                globals.brain_core_ws_url = switch_core_strdup(globals.pool, val);
            } else if (!strcmp(var, "audio-chunk-ms")) {
                globals.audio_chunk_ms = atoi(val);
            } else if (!strcmp(var, "sample-rate")) {
                globals.sample_rate = atoi(val);
            }
        }
    }

    switch_xml_free(xml);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Loaded config: brain_core_ws_url=%s, chunk_ms=%d, sample_rate=%d\n",
                     globals.brain_core_ws_url, globals.audio_chunk_ms, globals.sample_rate);

    return SWITCH_STATUS_SUCCESS;
}

/* Module load function */
SWITCH_MODULE_LOAD_FUNCTION(mod_audio_stream_load)
{
    switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;

    /* Initialize globals */
    memset(&globals, 0, sizeof(globals));
    globals.pool = pool;
    globals.audio_chunk_ms = 20;    // Default 20ms
    globals.sample_rate = 16000;    // Default 16kHz
    globals.channels = 1;            // Mono

    /* Load configuration */
    load_config();

    /* Initialize WebSocket context */
    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN; /* Client mode */
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    globals.ws_context = lws_create_context(&info);
    if (!globals.ws_context) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                         "Failed to create WebSocket context\n");
        return SWITCH_STATUS_FALSE;
    }

    /* Connect module interface */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    /* Register dialplan application */
    SWITCH_ADD_APP(app_interface, "stream_to_brain", "Stream audio to Brain-Core",
                   "Stream audio to Brain-Core Orchestrator", stream_to_brain_function, "",
                   SAF_SUPPORT_NOMEDIA);

    /* Register API command */
    SWITCH_ADD_API(api_interface, "audio_stream_status", "Check Brain-Core connection status",
                   audio_stream_status_api, "");

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "mod_audio_stream loaded successfully\n");

    return SWITCH_STATUS_SUCCESS;
}

/* Module shutdown function */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_audio_stream_shutdown)
{
    if (globals.ws_context) {
        lws_context_destroy(globals.ws_context);
        globals.ws_context = NULL;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "mod_audio_stream unloaded\n");

    return SWITCH_STATUS_SUCCESS;
}

/* Module runtime function */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_audio_stream_runtime)
{
    /* Service WebSocket events */
    while (globals.ws_context) {
        lws_service(globals.ws_context, 50);
    }

    return SWITCH_STATUS_TERM;
}
