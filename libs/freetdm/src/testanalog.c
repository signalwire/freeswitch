#include "openzap.h"
#include "zap_analog.h"


static void *test_call(zap_thread_t *me, void *obj)
{
	zap_channel_t *chan = (zap_channel_t *) obj;
	uint8_t frame[1024];
	zap_size_t len;


	zap_sleep(10 * 1000);
	
	zap_log(ZAP_LOG_DEBUG, "answer call and start echo test\n");

	zap_set_state_locked(chan, ZAP_CHANNEL_STATE_UP);
	zap_channel_command(chan, ZAP_COMMAND_SEND_DTMF, "5551212");

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

	return 0;
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

int main(int argc, char *argv[])
{
	zap_span_t *span;
	int span_id;
	zap_analog_data_t *analog_data;

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
	
	if (zap_analog_configure_span(span, "us", 2000, 11, on_signal) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Error configuring OpenZAP span\n");
		goto done;
	}
	analog_data = span->signal_data;
	zap_analog_start(span);

	while(zap_test_flag(analog_data, ZAP_ANALOG_RUNNING)) {
		zap_sleep(1 * 1000);
	}


 done:

	zap_global_destroy();

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

