#include "openzap.h"
#include "zap_ss7_boost.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
}

int main(int argc, char *argv[])
{
	zap_span_t *span;
	zap_ss7_boost_data_t *data;

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
	

	if (zap_ss7_boost_configure_span(span, "127.0.0.65", 53000, "127.0.0.66", 53000, on_signal) == ZAP_SUCCESS) {
		data = span->signal_data;
		zap_ss7_boost_start(span);
	} else {
		fprintf(stderr, "Error starting SS7_BOOST\n");
		goto done;
	}

	while(zap_test_flag(data, ZAP_SS7_BOOST_RUNNING)) {
		zap_sleep(1 * 1000);
	}

 done:

	zap_global_destroy();

}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
