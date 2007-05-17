#include "openzap.h"

int main(int argc, char *argv[])
{
	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");
	sleep(2);
	zap_global_destroy();

}
