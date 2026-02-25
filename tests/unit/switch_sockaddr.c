#include <switch.h>
#include <test/switch_test.h>

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_sockaddr)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_null_hostname_wildcard)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// NULL hostname should bind to wildcard address (0.0.0.0 or ::)
	status = switch_sockaddr_info_get(&sa, NULL, SWITCH_UNSPEC, 5060, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 5060);

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_numeric_ipv4)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	char ip_str[50] = {0};

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Test numeric IPv4 - should not perform DNS lookup
	status = switch_sockaddr_info_get(&sa, "127.0.0.1", SWITCH_UNSPEC, 8080, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 8080);

	// Verify the address is correct
	switch_get_addr(ip_str, sizeof(ip_str), sa);
	fst_check_string_equals(ip_str, "127.0.0.1");

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_numeric_ipv4_public)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	char ip_str[50] = {0};

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Test public DNS server IP (Google DNS)
	status = switch_sockaddr_info_get(&sa, "8.8.8.8", SWITCH_UNSPEC, 53, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 53);

	// Verify the address
	switch_get_addr(ip_str, sizeof(ip_str), sa);
	fst_check_string_equals(ip_str, "8.8.8.8");

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_numeric_ipv6_loopback)
{
	switch_memory_pool_t *pool = NULL;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

#if APR_HAVE_IPV6
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	// Test IPv6 loopback - should not perform DNS lookup
	status = switch_sockaddr_info_get(&sa, "::1", SWITCH_INET6, 5060, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET6);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 5060);
#else
	fst_check(1); // Skip test if IPv6 not supported
#endif

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_numeric_ipv6_with_brackets)
{
	switch_memory_pool_t *pool = NULL;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

#if APR_HAVE_IPV6
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	// Test IPv6 with brackets (URL format) - should not perform DNS lookup
	status = switch_sockaddr_info_get(&sa, "[::1]", SWITCH_INET6, 5060, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET6);
#else
	fst_check(1); // Skip test if IPv6 not supported
#endif

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_hostname_resolution_localhost)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Test hostname resolution - "localhost" should resolve
	status = switch_sockaddr_info_get(&sa, "localhost", SWITCH_UNSPEC, 9000, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 9000);

	// localhost should resolve to either 127.0.0.1 (IPv4) or ::1 (IPv6)
	fst_check(switch_sockaddr_get_family(sa) == AF_INET || switch_sockaddr_get_family(sa) == AF_INET6);

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_hostname_resolution_google_dns)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	int32_t family;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Test real DNS resolution - dns.google should resolve
	// This tests c-ares when available, APR otherwise
	status = switch_sockaddr_info_get(&sa, "dns.google", SWITCH_UNSPEC, 443, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 443);

	// Verify we got a valid address family (IPv4 or IPv6)
	family = switch_sockaddr_get_family(sa);
	fst_check(family == AF_INET || family == AF_INET6);

	// Log the resolver being used
#ifdef HAVE_CARES
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "DNS resolver: c-ares (async)\n");
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "DNS resolver: APR (blocking)\n");
#endif

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_invalid_hostname)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Test invalid/non-existent hostname - should fail
	status = switch_sockaddr_info_get(&sa, "this-hostname-definitely-does-not-exist-12345.invalid",
	                                   SWITCH_UNSPEC, 80, 0, pool);

	// Should return an error status (not SUCCESS)
	fst_check(status != SWITCH_STATUS_SUCCESS);

	// sa might be NULL or might point to error result depending on implementation
	// Either way is valid

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_family_preference_ipv4)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Request IPv4 specifically for localhost
	status = switch_sockaddr_info_get(&sa, "localhost", SWITCH_INET, 8080, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);
	fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET);
	fst_check_int_equals(switch_sockaddr_get_port(sa), 8080);

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_family_preference_ipv6)
{
	switch_memory_pool_t *pool = NULL;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

#if APR_HAVE_IPV6
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;

	// Request IPv6 specifically for localhost
	status = switch_sockaddr_info_get(&sa, "localhost", SWITCH_INET6, 8080, 0, pool);

	// Some systems might not have IPv6 localhost configured
	if (status == SWITCH_STATUS_SUCCESS) {
		fst_requires(sa != NULL);
		fst_check_int_equals(switch_sockaddr_get_family(sa), AF_INET6);
		fst_check_int_equals(switch_sockaddr_get_port(sa), 8080);
	}
#else
	fst_check(1); // Skip test if IPv6 not supported
#endif

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_port_variations)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	const switch_port_t test_ports[] = {0, 1, 80, 443, 5060, 8080, 65535};
	int i;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	for (i = 0; i < sizeof(test_ports) / sizeof(test_ports[0]); i++) {
		status = switch_sockaddr_info_get(&sa, "127.0.0.1", SWITCH_UNSPEC,
		                                   test_ports[i], 0, pool);
		fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
		fst_requires(sa != NULL);
		fst_check_int_equals(switch_sockaddr_get_port(sa), test_ports[i]);
	}

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_multiple_addresses)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	int32_t family;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// google.com typically returns multiple addresses
	// Test that we can at least get the first one
	status = switch_sockaddr_info_get(&sa, "google.com", SWITCH_UNSPEC, 80, 0, pool);
	fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	fst_requires(sa != NULL);

	// Verify the port is correct
	fst_check_int_equals(switch_sockaddr_get_port(sa), 80);

	// Verify we got a valid address family
	family = switch_sockaddr_get_family(sa);
	fst_check(family == AF_INET || family == AF_INET6);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                 "google.com resolved successfully\n");

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_TEST_BEGIN(test_performance_numeric_vs_dns)
{
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa = NULL;
	switch_status_t status;
	switch_time_t start, end;
	uint64_t numeric_time, dns_time;
	int i, iterations = 100;

	switch_core_new_memory_pool(&pool);
	fst_requires(pool != NULL);

	// Benchmark numeric IP (should be very fast - no DNS)
	start = switch_time_now();
	for (i = 0; i < iterations; i++) {
		status = switch_sockaddr_info_get(&sa, "8.8.8.8", SWITCH_UNSPEC, 53, 0, pool);
		fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	}
	end = switch_time_now();
	numeric_time = end - start;

	// Benchmark DNS resolution (slower - involves DNS lookup)
	start = switch_time_now();
	for (i = 0; i < iterations; i++) {
		status = switch_sockaddr_info_get(&sa, "localhost", SWITCH_UNSPEC, 80, 0, pool);
		fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
	}
	end = switch_time_now();
	dns_time = end - start;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
	                 "Performance (%d iterations): numeric IP = %" SWITCH_UINT64_T_FMT "us, "
	                 "DNS lookup = %" SWITCH_UINT64_T_FMT "us\n",
	                 iterations, numeric_time, dns_time);

	// Numeric IP resolution should be significantly faster than DNS
	// (This is informational - we don't enforce strict timing requirements)

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()
