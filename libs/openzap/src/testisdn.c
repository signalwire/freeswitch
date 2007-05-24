#include "openzap.h"
#include "zap_isdn.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
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
		goto done;
	}
	
	
	if (zap_isdn_configure_span(span, Q931_TE, Q931_Dialect_National, on_signal) == ZAP_SUCCESS) {
		zap_isdn_start(span);
	} else {
		fprintf(stderr, "Error starting ISDN D-Channel\n");
		goto done;
	}

	while(zap_test_flag(span->isdn_data, ZAP_ISDN_RUNNING)) {
		sleep(1);
	}

 done:

	zap_global_destroy();

}
