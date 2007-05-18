#include "openzap.h"

int main(int argc, char *argv[])
{
	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);
	zap_channel_t *chan;
	unsigned ms = 20;
	
	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	
	if (zap_channel_open("wanpipe", 1, 1, &chan) == ZAP_SUCCESS) {
		int x = 0;

		if (zap_channel_command(chan, ZAP_COMMAND_SET_INTERVAL, &ms) == ZAP_SUCCESS) {
			zap_channel_command(chan, ZAP_COMMAND_SET_INTERVAL, &ms);
			printf("interval set to %u\n", ms);
		} else {
			printf("set interval failed\n");
		}

		for(x = 0; x < 25; x++) {
			unsigned char buf[80];
			zap_size_t len = sizeof(buf);
			zap_wait_flag_t flags = ZAP_READ;

			zap_channel_wait(chan, &flags, 0);
			if (flags & ZAP_READ) {
				if (zap_channel_read(chan, buf, &len) == ZAP_SUCCESS) {
					printf("READ: %d\n", len); 
				} else {
					printf("READ FAIL! %d\n", len);
					break;
				}
			} else {
				printf("wait fail\n");
			}
		}
	}

	zap_global_destroy();

}
