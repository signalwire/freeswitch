#include "ks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

int main(int argc, char **argv)
{
	ks_pool_t *pool;
	ks_network_list_t *list = NULL;
	ks_bool_t match;

	ks_init();

	plan(8);

	ks_pool_open(&pool);

	ks_network_list_create(&list, "test", KS_FALSE, pool);

	
	ks_network_list_add_cidr(list, "10.0.0.0/8", KS_TRUE);
	ks_network_list_add_cidr(list, "172.16.0.0/12", KS_TRUE);
	ks_network_list_add_cidr(list, "192.168.0.0/16", KS_TRUE);
	ks_network_list_add_cidr(list, "fe80::/10", KS_TRUE);


	match = ks_check_network_list_ip("192.168.1.1", list);
	ok(match);

	match = ks_check_network_list_ip("208.34.128.7", list);
	ok (!match);

	match = ks_check_network_list_ip_cidr("192.168.1.1", "192.168.0.0/16");
	ok(match);

	match = ks_check_network_list_ip_cidr("208.34.128.7", "192.168.0.0/16");
	ok (!match);


	ks_pool_free(&list);


	ks_network_list_create(&list, "test", KS_TRUE, pool);

	ks_network_list_add_cidr(list, "0.0.0.0/0", KS_FALSE);
	ks_network_list_add_cidr(list, "fe80::/10", KS_FALSE);

	
	match = ks_check_network_list_ip("2637:f368:1281::10", list);
	ok(match);

	match = ks_check_network_list_ip("fe80::18b7:53b3:51d8:f5cf", list);
	ok(!match);

	match = ks_check_network_list_ip_cidr("fe80::18b7:53b3:51d8:f5cf", "fe80::/10");
	ok(match);

	match = ks_check_network_list_ip_cidr("2637:f368:1281::10", "fe80::/10");
	ok(!match);

	ks_pool_free(&list);

	ks_pool_close(&pool);

	ks_shutdown();

	done_testing();
}
