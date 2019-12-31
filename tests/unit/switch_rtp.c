
#include <switch.h>
#include <test/switch_test.h>

static const char *rx_host = "127.0.0.1";
static switch_port_t rx_port = 12346;
static const char *tx_host = "127.0.0.1";
static switch_port_t tx_port = 54320;
static switch_memory_pool_t *pool = NULL;
static switch_rtp_t *rtp_session = NULL;
static switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
const char *err = NULL;
static const switch_payload_t TEST_PT = 8;
switch_rtp_packet_t rtp_packet;
switch_frame_flag_t *frame_flags;
switch_io_flag_t io_flags;
switch_payload_t read_pt;
uint datalen;

FST_CORE_BEGIN("./conf")
{
FST_SUITE_BEGIN(switch_rtp)
{
FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_rtp)
{
	switch_core_new_memory_pool(&pool);
	
	rtp_session = switch_rtp_new(rx_host, rx_port, tx_host, tx_port, TEST_PT, 8000, 20 * 1000, flags, "soft", &err, pool, 0, 0);
	fst_xcheck(rtp_session != NULL, "get RTP session");
	fst_requires(rtp_session);
	fst_requires(switch_rtp_ready(rtp_session));
	switch_rtp_activate_rtcp(rtp_session, 5, rx_port + 1, 0);
	switch_rtp_set_default_payload(rtp_session, TEST_PT);
	fst_xcheck(switch_rtp_get_default_payload(rtp_session) == TEST_PT, "get Payload Type")
	switch_rtp_set_ssrc(rtp_session, 0xabcd);
	switch_rtp_set_remote_ssrc(rtp_session, 0xcdef);
	fst_xcheck(switch_rtp_get_ssrc(rtp_session) == 0xabcd, "get SSRC");
	switch_rtp_stats_t *stats = switch_rtp_get_stats(rtp_session, pool);
	fst_requires(stats);
	switch_rtp_destroy(&rtp_session);

	switch_core_destroy_memory_pool(&pool);
}
FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()

