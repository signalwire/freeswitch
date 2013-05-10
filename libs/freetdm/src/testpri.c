#include "freetdm.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

static int THREADS[4][31] = { {0} };
static int R = 0;
static int T = 0;
static ftdm_mutex_t *mutex = NULL;


static void *channel_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = obj;
	int fd = -1;
	short buf[160];
	int spanid = ftdm_channel_get_span_id(ftdmchan);
	int chanid = ftdm_channel_get_id(ftdmchan);

	ftdm_unused_arg(me);

	ftdm_mutex_lock(mutex);
	T++;
	ftdm_mutex_unlock(mutex);

	ftdm_channel_call_answer(ftdmchan);

	if ((fd = open("test.raw", O_RDONLY, 0)) < 0) {
		goto end;
	}

	while(R == 1 && THREADS[spanid][chanid] == 1) {
		ssize_t bytes = read(fd, buf, sizeof(buf));
		size_t bbytes;

		if (bytes <= 0) {
			break;
		}

		bbytes = (size_t) bytes;

		fio_slin2alaw(buf, sizeof(buf), &bbytes);

		if (ftdm_channel_write(ftdmchan, buf, sizeof(buf), &bbytes) != FTDM_SUCCESS) {
			break;
		}
	}

	close(fd);

 end:

	ftdm_channel_call_hangup(ftdmchan);

	THREADS[spanid][chanid] = 0;

	ftdm_mutex_lock(mutex);
	T = 0;
	ftdm_mutex_unlock(mutex);
	
	return NULL;
}

static FIO_SIGNAL_CB_FUNCTION(on_signal)
{
	int spanid = ftdm_channel_get_span_id(sigmsg->channel);
	int chanid = ftdm_channel_get_id(sigmsg->channel);
	ftdm_log(FTDM_LOG_DEBUG, "got sig %d:%d [%s]\n", spanid, chanid, ftdm_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {

	case FTDM_SIGEVENT_STOP:
		THREADS[spanid][chanid] = -1;
		break;

	case FTDM_SIGEVENT_START:
		if (!THREADS[spanid][chanid]) {
			THREADS[spanid][chanid] = 1;
			ftdm_thread_create_detached(channel_run, sigmsg->channel);
		}
		
		break;
	default:
		break;
	}
	
	return FTDM_SUCCESS;
}


static void handle_SIGINT(int sig)
{
	ftdm_unused_arg(sig);

	ftdm_mutex_lock(mutex);
	R = 0;
	ftdm_mutex_unlock(mutex);

	return;
}

int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	ftdm_mutex_create(&mutex);

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (argc < 2) {
		printf("umm no\n");
		exit(-1);
	}

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(-1);
	}

	printf("FreeTDM loaded\n");

	if (ftdm_span_find(atoi(argv[1]), &span) != FTDM_SUCCESS) {
		fprintf(stderr, "Error finding FreeTDM span\n");
		goto done;
	}
	


	if (ftdm_configure_span(
						   span, "libpri", on_signal,
						   "node", "cpe",
						   "switch", "euroisdn",
						   "dp", "unknown",
						   "l1", "alaw",
						   "debug", NULL,
						   "opts", 0,
						   FTDM_TAG_END) == FTDM_SUCCESS) {
						   

		ftdm_span_start(span);
	} else {
		fprintf(stderr, "Error starting ISDN D-Channel\n");
		goto done;
	}

	signal(SIGINT, handle_SIGINT);
	ftdm_mutex_lock(mutex);
	R = 1;
	ftdm_mutex_unlock(mutex);
	while(R || T) {
		ftdm_sleep(1 * 1000);
	}

 done:

	ftdm_global_destroy();

	return 1;

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
