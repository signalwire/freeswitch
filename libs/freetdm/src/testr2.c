#include "freetdm.h"
#include <signal.h>
#include <stdlib.h>

static volatile int running = 0;
static ftdm_mutex_t *the_mutex = NULL;
static ftdm_channel_t *fchan = NULL;
static ftdm_channel_indication_t indication = FTDM_CHANNEL_INDICATE_NONE;

static FIO_SIGNAL_CB_FUNCTION(on_r2_signal)
{
	int chanid = ftdm_channel_get_ph_id(sigmsg->channel);
	ftdm_log(FTDM_LOG_DEBUG, "Got R2 channel sig [%s] in channel %d\n", ftdm_signal_event2str(sigmsg->event_id), chanid);
	switch (sigmsg->event_id) {
	case FTDM_SIGEVENT_START:
		{
			ftdm_mutex_lock(the_mutex);
			if (!fchan) {
				fchan = sigmsg->channel;
				indication = FTDM_CHANNEL_INDICATE_PROCEED;
			}
			ftdm_mutex_unlock(the_mutex);
		}
		break;
	case FTDM_SIGEVENT_INDICATION_COMPLETED:
		{
			ftdm_channel_indication_t ind = FTDM_CHANNEL_INDICATE_NONE;
			if (sigmsg->ev_data.indication_completed.indication == FTDM_CHANNEL_INDICATE_PROCEED) {
				ftdm_log(FTDM_LOG_DEBUG, "Proceed indication result = %d\n", sigmsg->ev_data.indication_completed.status);
				ind = FTDM_CHANNEL_INDICATE_PROGRESS;
			} else if (sigmsg->ev_data.indication_completed.indication == FTDM_CHANNEL_INDICATE_PROGRESS) {
				ftdm_log(FTDM_LOG_DEBUG, "Progress indication result = %d\n", sigmsg->ev_data.indication_completed.status);
				ind = FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA;
			} else if (sigmsg->ev_data.indication_completed.indication == FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA) {
				ftdm_log(FTDM_LOG_DEBUG, "Progress media indication result = %d\n", sigmsg->ev_data.indication_completed.status);
				ind = FTDM_CHANNEL_INDICATE_ANSWER;
			} else if (sigmsg->ev_data.indication_completed.indication == FTDM_CHANNEL_INDICATE_ANSWER) {
				ftdm_log(FTDM_LOG_DEBUG, "Answer indication result = %d\n", sigmsg->ev_data.indication_completed.status);
			} else {
				ftdm_log(FTDM_LOG_DEBUG, "Unexpected indication, result = %d\n", sigmsg->ev_data.indication_completed.status);
				exit(1);
			}
			ftdm_mutex_lock(the_mutex);
			if (fchan) {
				indication = ind;
			}
			ftdm_mutex_unlock(the_mutex);
		}
		break;
	case FTDM_SIGEVENT_STOP:
		{
			ftdm_channel_call_hangup(sigmsg->channel);
		}
		break;
	case FTDM_SIGEVENT_RELEASED:
		{
			ftdm_mutex_lock(the_mutex);
			if (fchan && fchan == sigmsg->channel) {
				fchan = NULL;
			}
			ftdm_mutex_unlock(the_mutex);
		}
		break;
	default:
		break;
	}
	return FTDM_SUCCESS;
}

static void stop_test(int sig)
{
	ftdm_unused_arg(sig);
	running = 0;
}

int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	ftdm_conf_parameter_t parameters[20];
	
	ftdm_mutex_create(&the_mutex);

	if (argc < 2) {
		printf("umm no\n");
		exit(1);
	}

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(1);
	}

	ftdm_global_configuration();

	printf("FreeTDM loaded\n");

	if (ftdm_span_find_by_name(argv[1], &span) != FTDM_SUCCESS) {
		fprintf(stderr, "Error finding FreeTDM span %s\n", argv[1]);
		goto done;
	}
	
	/* testing non-blocking operation */
	//ftdm_span_set_blocking_mode(span, FTDM_FALSE);

	parameters[0].var = "variant";
	parameters[0].val = "br";

	parameters[1].var = "max_ani";
	parameters[1].val = "4";

	parameters[2].var = "max_dnis";
	parameters[2].val = "4";

	parameters[3].var = "logging";
	parameters[3].val = "all";

	parameters[4].var = NULL;
	parameters[4].val = NULL;

	if (ftdm_configure_span_signaling(span, "r2", on_r2_signal, parameters) == FTDM_SUCCESS) {
		ftdm_span_start(span);
	} else {
		fprintf(stderr, "Error starting R2 span\n");
		goto done;
	}

	running = 1;
	signal(SIGINT, stop_test);
	while(running) {
		ftdm_sleep(20);
		if (fchan && indication != FTDM_CHANNEL_INDICATE_NONE) {
			ftdm_channel_t *lchan = NULL;
			ftdm_channel_indication_t ind = FTDM_CHANNEL_INDICATE_NONE;
			ftdm_time_t start, stop, diff;

			ftdm_mutex_lock(the_mutex);
			ind = indication;
			indication = FTDM_CHANNEL_INDICATE_NONE;
			lchan = fchan;
			ftdm_mutex_unlock(the_mutex);

			start = ftdm_current_time_in_ms();
			ftdm_channel_call_indicate(lchan, ind);
			stop = ftdm_current_time_in_ms();
			diff = stop - start;
			ftdm_log(FTDM_LOG_DEBUG, "Setting indication %s took %"FTDM_TIME_FMT" ms\n",
					ftdm_channel_indication2str(ind), diff);
		}
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
