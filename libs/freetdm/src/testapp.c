#include "openzap.h"

int main(int argc, char *argv[])
{
	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);
	zap_channel_t *chan;
	unsigned ms = 20;
	zap_codec_t codec = ZAP_CODEC_SLIN;
	
	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	
	if (zap_channel_open("wanpipe", 1, 1, &chan) == ZAP_SUCCESS) {
		int x = 0;

		if (zap_channel_command(chan, ZAP_COMMAND_SET_INTERVAL, &ms) == ZAP_SUCCESS) {
			ms = 0;
			zap_channel_command(chan, ZAP_COMMAND_GET_INTERVAL, &ms);
			printf("interval set to %u\n", ms);
		} else {
			printf("set interval failed [%s]\n", chan->last_error);
		}

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
			
			if (zap_channel_wait(chan, &flags, 0) == ZAP_FAIL) {
				printf("wait FAIL! %d [%s]\n", len, chan->last_error);
			}
			if (flags & ZAP_READ) {
				if (zap_channel_read(chan, buf, &len) == ZAP_SUCCESS) {
					printf("READ: %d\n", len); 
				} else {
					printf("READ FAIL! %d [%s]\n", len, chan->last_error);
					break;
				}
			} else {
				printf("wait fail [%s]\n", chan->last_error);
			}
		}
	}

	zap_global_destroy();

}
