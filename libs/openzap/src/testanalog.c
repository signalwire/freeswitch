#include "openzap.h"


static void *test_call(zap_thread_t *me, void *obj)
{
	zap_channel_t *chan = (zap_channel_t *) obj;
	uint8_t frame[1024];
	zap_size_t len;
	char *number = strdup("5551212");

	zap_sleep(10 * 1000);
	
	zap_log(ZAP_LOG_DEBUG, "answer call and start echo test\n");

	zap_set_state_locked(chan, ZAP_CHANNEL_STATE_UP);
	zap_channel_command(chan, ZAP_COMMAND_SEND_DTMF, number);

	while (chan->state == ZAP_CHANNEL_STATE_UP) {
		zap_wait_flag_t flags = ZAP_READ;
		
		if (zap_channel_wait(chan, &flags, -1) == ZAP_FAIL) {
			break;
		}
		len = sizeof(frame);
		if (flags & ZAP_READ) {
			if (zap_channel_read(chan, frame, &len) == ZAP_SUCCESS) {
				//zap_log(ZAP_LOG_DEBUG, "WRITE %d\n", len);
				zap_channel_write(chan, frame, sizeof(frame), &len);
			} else {
				break;
			}
		}
	}
	
	if (chan->state == ZAP_CHANNEL_STATE_UP) {
		zap_set_state_locked(chan, ZAP_CHANNEL_STATE_BUSY);
	}

	zap_log(ZAP_LOG_DEBUG, "call over\n");
	free(number);
	return NULL;
}

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	zap_log(ZAP_LOG_DEBUG, "got sig [%s]\n", zap_signal_event2str(sigmsg->event_id));

	switch(sigmsg->event_id) {
	case ZAP_SIGEVENT_START:
		zap_set_state_locked(sigmsg->channel, ZAP_CHANNEL_STATE_RING);
		zap_log(ZAP_LOG_DEBUG, "launching thread and indicating ring\n");
		zap_thread_create_detached(test_call, sigmsg->channel);
		break;
	default:
		break;
	}

	return ZAP_SUCCESS;
}

static int R = 0;
#if 0
static void handle_SIGINT(int sig)
{
	if (sig);
	R = 0;
	return;
}
#endif

int main(int argc, char *argv[])
{
	zap_span_t *span;
	int span_id;
	int digit_timeout = 2000;
	int max_dialstr = 11;

	if (argc < 2) {
		printf("usage %s <spanno>\n", argv[0]);
		exit(-1);
	}

	span_id = atoi(argv[1]);

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (zap_global_init() != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Error loading OpenZAP\n");
		exit(-1);
	}

	zap_log(ZAP_LOG_DEBUG, "OpenZAP loaded\n");

	if (zap_span_find(span_id, &span) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span\n");
		goto done;
	}
	

	if (zap_configure_span("analog", span, on_signal, 
						   "tonemap", "us", 
						   "digit_timeout", &digit_timeout,
						   "max_dialstr", &max_dialstr,
						   TAG_END
						   ) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Error configuring OpenZAP span\n");
		goto done;
	}
	zap_span_start(span);
	
	R = 1;

	while(zap_running() && R) {
		zap_sleep(1 * 1000);
	}

done:

	zap_global_destroy();
	return 0;
}

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

