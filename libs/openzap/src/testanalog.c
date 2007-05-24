#include "openzap.h"
#include "zap_analog.h"

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
	}
	
	
	zap_analog_configure_span(span, on_signal);
	zap_analog_start(span);

	while(zap_test_flag(span->analog_data, ZAP_ANALOG_RUNNING)) {
		sleep(1);
	}

	zap_global_destroy();

}
