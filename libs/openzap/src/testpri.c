#include "openzap.h"
#include <signal.h>

static int THREADS[4][31] = { {0} };
static int R = 0;
static int T = 0;
static zap_mutex_t *mutex = NULL;


static void *channel_run(zap_thread_t *me, void *obj)
{
	zap_channel_t *zchan = obj;
	int fd = -1;
	short buf[160];

	zap_mutex_lock(mutex);
	T++;
	zap_mutex_unlock(mutex);

	zap_set_state_locked_wait(zchan, ZAP_CHANNEL_STATE_UP);

	if ((fd = open("test.raw", O_RDONLY, 0)) < 0) {
		goto end;
	}

	while(R == 1 && THREADS[zchan->span_id][zchan->chan_id] == 1) {
		ssize_t bytes = read(fd, buf, sizeof(buf));
		size_t bbytes;

		if (bytes <= 0) {
			break;
		}

		bbytes = (size_t) bytes;

		zio_slin2alaw(buf, sizeof(buf), &bbytes);

		if (zap_channel_write(zchan, buf, sizeof(buf), &bbytes) != ZAP_SUCCESS) {
			break;
		}
	}

	close(fd);

 end:

	zap_set_state_locked_wait(zchan, ZAP_CHANNEL_STATE_HANGUP);

	THREADS[zchan->span_id][zchan->chan_id] = 0;

	zap_mutex_lock(mutex);
	T = 0;
	zap_mutex_unlock(mutex);
	
	return NULL;
}

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	zap_log(ZAP_LOG_DEBUG, "got sig %d:%d [%s]\n", sigmsg->channel->span_id, sigmsg->channel->chan_id, zap_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {

	case ZAP_SIGEVENT_STOP:
		THREADS[sigmsg->channel->span_id][sigmsg->channel->chan_id] = -1;
		break;

	case ZAP_SIGEVENT_START:
		if (!THREADS[sigmsg->channel->span_id][sigmsg->channel->chan_id]) {
			THREADS[sigmsg->channel->span_id][sigmsg->channel->chan_id] = 1;
			zap_thread_create_detached(channel_run, sigmsg->channel);
		}
		
		break;
	default:
		break;
	}
	
	return ZAP_SUCCESS;
}


static void handle_SIGINT(int sig)
{
	if (sig);

	zap_mutex_lock(mutex);
	R = 0;
	zap_mutex_unlock(mutex);

	return;
}

int main(int argc, char *argv[])
{
	zap_span_t *span;
	zap_mutex_create(&mutex);

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (argc < 2) {
		printf("umm no\n");
		exit(-1);
	}

	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	if (zap_span_find(atoi(argv[1]), &span) != ZAP_SUCCESS) {
		fprintf(stderr, "Error finding OpenZAP span\n");
		goto done;
	}
	


	if (zap_configure_span(
						   "libpri", span, on_signal,
						   "node", "cpe",
						   "switch", "euroisdn",
						   "dp", "unknown",
						   "l1", "alaw",
						   "debug", NULL,
						   "opts", 0,
						   TAG_END) == ZAP_SUCCESS) {
						   

		zap_span_start(span);
	} else {
		fprintf(stderr, "Error starting ISDN D-Channel\n");
		goto done;
	}

	signal(SIGINT, handle_SIGINT);
	zap_mutex_lock(mutex);
	R = 1;
	zap_mutex_unlock(mutex);
	while(R || T) {
		zap_sleep(1 * 1000);
	}

 done:

	zap_global_destroy();

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
