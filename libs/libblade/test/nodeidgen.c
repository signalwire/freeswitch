#include <ks.h>
#include <ks_dht.h>
#include <ks_dht-int.h>
#include <tap.h>

int main() {
	uint8_t nodeid[KS_DHT_NODEID_SIZE];
	char buf[KS_DHT_NODEID_SIZE * 2 + 1];
	
	randombytes_buf(nodeid, KS_DHT_NODEID_SIZE);

	ks_dht_hex(nodeid, buf, KS_DHT_NODEID_SIZE);

	printf(buf);
	return 0;
}
