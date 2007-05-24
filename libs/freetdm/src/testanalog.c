#include "openzap.h"
#include "zap_analog.h"


static void *test_call(zap_thread_t *me, void *obj)
{
	zap_channel_t *chan = (zap_channel_t *) obj;
	uint8_t frame[1024];
	zap_size_t len;


	sleep(5);
	
	printf("answer call\n");

	zap_set_state_locked(chan, ZAP_CHANNEL_STATE_UP);
	
	while (chan->state == ZAP_CHANNEL_STATE_UP) {
		zap_wait_flag_t flags = ZAP_READ;
		
		if (zap_channel_wait(chan, &flags, -1) == ZAP_FAIL) {
			break;
		}
		len = sizeof(frame);
		if (flags & ZAP_READ) {
			if (zap_channel_read(chan, frame, &len) == ZAP_SUCCESS) {
				//printf("WRITE %d\n", len);
				zap_channel_write(chan, frame, &len);
			} else {
				break;
			}
		}
	}
	
	if (chan->state == ZAP_CHANNEL_STATE_UP) {
		zap_set_state_locked(chan, ZAP_CHANNEL_STATE_BUSY);
	}

	printf("call over\n");

}

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	printf("got sig [%s]\n", zap_signal_event2str(sigmsg->event_id));

	switch(sigmsg->event_id) {
	case ZAP_SIGEVENT_START:
		zap_set_state_locked(sigmsg->channel, ZAP_CHANNEL_STATE_RING);
		printf("launching thread and indicating ring\n");
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

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	if (zap_span_find("wanpipe", 1, &span) != ZAP_SUCCESS) {
		fprintf(stderr, "Error finding OpenZAP span\n");
	}
	
	
	zap_analog_configure_span(span, "us", 2000, 11, on_signal);
	zap_analog_start(span);

	while(zap_test_flag(span->analog_data, ZAP_ANALOG_RUNNING)) {
		sleep(1);
	}

	zap_global_destroy();

}
