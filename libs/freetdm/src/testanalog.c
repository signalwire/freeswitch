#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freetdm.h"


static void *test_call(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *chan = (ftdm_channel_t *) obj;
	uint8_t frame[1024];
	ftdm_size_t len;
	char *number = ftdm_strdup("5551212");

	ftdm_unused_arg(me);

	ftdm_sleep(10 * 1000);
	
	ftdm_log(FTDM_LOG_DEBUG, "answer call and start echo test\n");

	ftdm_channel_call_answer(chan);
	ftdm_channel_command(chan, FTDM_COMMAND_SEND_DTMF, number);

	while (ftdm_channel_call_check_answered(chan)) {
		ftdm_wait_flag_t flags = FTDM_READ;
		
		if (ftdm_channel_wait(chan, &flags, -1) == FTDM_FAIL) {
			break;
		}
		len = sizeof(frame);
		if (flags & FTDM_READ) {
			if (ftdm_channel_read(chan, frame, &len) == FTDM_SUCCESS) {
				//ftdm_log(FTDM_LOG_DEBUG, "WRITE %d\n", len);
				ftdm_channel_write(chan, frame, sizeof(frame), &len);
			} else {
				break;
			}
		}
	}
	
	if (ftdm_channel_call_check_answered(chan)) {
		ftdm_channel_call_indicate(chan, FTDM_CHANNEL_INDICATE_BUSY);
	}

	ftdm_log(FTDM_LOG_DEBUG, "call over\n");
	ftdm_safe_free(number);
	return NULL;
}

static FIO_SIGNAL_CB_FUNCTION(on_signal)
{
	ftdm_log(FTDM_LOG_DEBUG, "got sig [%s]\n", ftdm_signal_event2str(sigmsg->event_id));

	switch(sigmsg->event_id) {
	case FTDM_SIGEVENT_START:
		ftdm_channel_call_indicate(sigmsg->channel, FTDM_CHANNEL_INDICATE_RINGING);
		ftdm_log(FTDM_LOG_DEBUG, "launching thread and indicating ring\n");
		ftdm_thread_create_detached(test_call, sigmsg->channel);
		break;
	default:
		break;
	}

	return FTDM_SUCCESS;
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
	ftdm_span_t *span;
	int span_id;
	int digit_timeout = 2000;
	int max_dialstr = 11;

	if (argc < 2) {
		printf("usage %s <spanno>\n", argv[0]);
		exit(-1);
	}

	span_id = atoi(argv[1]);

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (ftdm_global_init() != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading FreeTDM\n");
		exit(-1);
	}

	ftdm_log(FTDM_LOG_DEBUG, "FreeTDM loaded\n");

	if (ftdm_span_find(span_id, &span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span\n");
		goto done;
	}
	

	if (ftdm_configure_span(span, "analog", on_signal, 
						   "tonemap", "us", 
						   "digit_timeout", &digit_timeout,
						   "max_dialstr", &max_dialstr,
						   FTDM_TAG_END
						   ) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM span\n");
		goto done;
	}
	ftdm_span_start(span);
	
	R = 1;

	while(ftdm_running() && R) {
		ftdm_sleep(1 * 1000);
	}

done:

	ftdm_global_destroy();
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

