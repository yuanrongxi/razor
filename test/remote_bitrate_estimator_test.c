#include "remote_bitrate_estimator.h"
#include "common_test.h"

#define k_rtt_ms		200
#define k_min_bitrate	80000
#define k_max_bitrate	320000

#define k_packet_count	200
#define k_payload_size  1000

static void test_no_crach()
{
	int i, bitrate;
	int64_t now_ts;
	uint32_t timestamp;
	bin_stream_t strm;

	remote_bitrate_estimator_t* est = rbe_create();

	now_ts = GET_SYS_MS();
	timestamp = 1000;

	rbe_update_rtt(est, k_rtt_ms);
	rbe_set_min_bitrate(est, k_min_bitrate);
	rbe_set_max_bitrate(est, k_max_bitrate);

	for (i = 0; i < k_packet_count; ++i){
		rbe_incoming_packet(est, timestamp, now_ts, k_payload_size, now_ts);
		now_ts += 100;
		timestamp += 100;
		rbe_heartbeat(est, now_ts, &strm);
	}

	assert(rbe_last_estimate(est, &bitrate) == 0);

	rbe_destroy(est);
}

static void test_overuse_bitrate()
{
	int i, bitrate;
	int64_t now_ts;
	uint32_t timestamp;
	bin_stream_t strm;
	remote_bitrate_estimator_t* est = rbe_create();

	now_ts = GET_SYS_MS();
	timestamp = 1000;

	rbe_update_rtt(est, k_rtt_ms);
	rbe_set_min_bitrate(est, k_min_bitrate);
	rbe_set_max_bitrate(est, k_max_bitrate);

	for (i = 0; i < k_packet_count; ++i){
		rbe_incoming_packet(est, timestamp, now_ts, k_payload_size, now_ts);
		now_ts += 52;
		timestamp += 50;
		rbe_heartbeat(est, now_ts, &strm);
	}

	assert(rbe_last_estimate(est, &bitrate) == 0);
	assert(est->detector->state == kBwOverusing);

	EXPECT_LT(bitrate, 20 * 1000 * 8);

	rbe_destroy(est);
}

static void test_normal_bitrate()
{
	int i, bitrate;
	int64_t now_ts;
	uint32_t timestamp;
	bin_stream_t strm;

	remote_bitrate_estimator_t* est = rbe_create();

	now_ts = GET_SYS_MS();
	timestamp = 1000;

	rbe_update_rtt(est, k_rtt_ms);
	rbe_set_min_bitrate(est, k_min_bitrate);
	rbe_set_max_bitrate(est, k_max_bitrate);

	for (i = 0; i < k_packet_count; ++i){
		rbe_incoming_packet(est, timestamp, now_ts, k_payload_size, now_ts);
		if (i % 2 == 0)
			now_ts += 52;
		else
			now_ts += 49;

		timestamp += 50;
		rbe_heartbeat(est, now_ts, &strm);
	}

	assert(rbe_last_estimate(est, &bitrate) == 0);
	assert(est->detector->state == kBwNormal);

	rbe_destroy(est);
}

void test_rbe()
{
	test_no_crach();
	test_overuse_bitrate();
	test_normal_bitrate();
}





