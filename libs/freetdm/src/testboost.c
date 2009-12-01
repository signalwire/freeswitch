#include "openzap.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
}

static int R = 0;
#if 0
static void handle_SIGINT(int sig)
{
	if (sig);
	R = 0;
	return;
}
#endif
int main(int argc, char *argv[])
{
	zap_conf_parameter_t parameters[20];
	zap_span_t *span;
	int local_port, remote_port;

	local_port = remote_port = 53000;

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);
#if 0
	if (argc < 2) {
		printf("invalid arguments\n");
		exit(-1);
	}
#endif

	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}
	if (zap_global_configuration() != ZAP_SUCCESS) {
		fprintf(stderr, "Error configuring OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	if (zap_span_find_by_name("wp1", &span) != ZAP_SUCCESS) {
		fprintf(stderr, "Error finding OpenZAP span %s\n", argv[1]);
		goto done;
	}
	parameters[0].var = "sigmod";	
	parameters[0].val = "sangoma_prid";	
	parameters[1].var = "switchtype";	
	parameters[1].val = "euroisdn";	
	parameters[1].var = "signalling";	
	parameters[1].val = "pri_cpe";	
	parameters[2].var = NULL;
	if (zap_configure_span_signaling("sangoma_boost", span, on_signal, parameters) == ZAP_SUCCESS) {
		zap_span_start(span);
	} else {
		fprintf(stderr, "Error starting SS7_BOOST\n");
		goto done;
	}

	while(zap_running() && R) {
		zap_sleep(1 * 1000);
	}

 done:

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
