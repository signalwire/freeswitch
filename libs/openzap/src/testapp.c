#include "openzap.h"

int main(int argc, char *argv[])
{
	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);
	zap_channel_t *chan;
	unsigned ms = 20;
	zap_codec_t codec = ZAP_CODEC_SLIN;
	unsigned runs = 1;


	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

 top:
	//if (zap_channel_open_any("wanpipe", 0, ZAP_TOP_DOWN, &chan) == ZAP_SUCCESS) {
	if (zap_channel_open(1, 1, &chan) == ZAP_SUCCESS) {
		int x = 0;
		printf("opened channel %d:%d\n", chan->span_id, chan->chan_id);

#if 1
		if (zap_channel_command(chan, ZAP_COMMAND_SET_INTERVAL, &ms) == ZAP_SUCCESS) {
			ms = 0;
			zap_channel_command(chan, ZAP_COMMAND_GET_INTERVAL, &ms);
			printf("interval set to %u\n", ms);
		} else {
			printf("set interval failed [%s]\n", chan->last_error);
		}
#endif
		if (zap_channel_command(chan, ZAP_COMMAND_SET_CODEC, &codec) == ZAP_SUCCESS) {
			codec = 1;
			zap_channel_command(chan, ZAP_COMMAND_GET_CODEC, &codec);
			printf("codec set to %u\n", codec);
		} else {
			printf("set codec failed [%s]\n", chan->last_error);
		}

		for(x = 0; x < 25; x++) {
			unsigned char buf[2048];
			zap_size_t len = sizeof(buf);
			zap_wait_flag_t flags = ZAP_READ;

			if (zap_channel_wait(chan, &flags, -1) == ZAP_FAIL) {
				printf("wait FAIL! %u [%s]\n", (unsigned)len, chan->last_error);
			}
			if (flags & ZAP_READ) {
				if (zap_channel_read(chan, buf, &len) == ZAP_SUCCESS) {
					printf("READ: %u\n", (unsigned)len); 
				} else {
					printf("READ FAIL! %u [%s]\n", (unsigned)len, chan->last_error);
					break;
				}
			} else {
				printf("wait fail [%s]\n", chan->last_error);
			}
		}
		zap_channel_close(&chan);
	} else {
		printf("open fail [%s]\n", chan->last_error);
	}

	if(--runs) {
		goto top;
	}

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
