#include "openzap.h"

int main(int argc, char *argv[])
{
	printf("hello\n");

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);
	zap_global_init();
}
