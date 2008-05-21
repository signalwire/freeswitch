#include "openzap.h"
#include "zap_isdn.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
}

int main(int argc, char *argv[])
{
	zap_span_t *span;
	zap_isdn_data_t *data;

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
	
	
	if (zap_isdn_configure_span(span, Q931_TE, Q931_Dialect_National, 0, on_signal) == ZAP_SUCCESS) {
		data = span->signal_data;
		zap_isdn_start(span);
	} else {
		fprintf(stderr, "Error starting ISDN D-Channel\n");
		goto done;
	}

	while(zap_test_flag(data, ZAP_ISDN_RUNNING)) {
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
