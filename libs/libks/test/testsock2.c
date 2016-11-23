#include <ks.h>
#include <tap.h>

int main(int argc, char *argv[])
{
	char ip[80] = "";

	ks_init();

	if (argc > 1) {
		ks_ip_route(ip, sizeof(ip), argv[1]);
		printf("IPS [%s]\n", ip);
	} else {
		fprintf(stderr, "Missing arg <ip>\n");
	}

	ks_shutdown();

	done_testing();
}
