#include "common_test.h"
#include "bbr_bandwidth_sample.h"

#define kRegularPacketSizeBytes 1280

typedef struct
{
	bbr_bandwidth_sampler_t*		sampler;
	int64_t							now_ts;
	size_t							bytes_in_flight;
}sample_tester_t;

static void sample_tester_init(sample_tester_t* test)
{
	test->sampler = sampler_create();
	test->now_ts = 0;
	test->bytes_in_flight = 0;
}

static void sample_tester_destroy(sample_tester_t* test)
{
	if (test->sampler != NULL){
		sampler_destroy(test->sampler);
		test->sampler = NULL;
	}
}

static void sample_tester_send_packet(sample_tester_t* test, int64_t number)
{
	sampler_on_packet_sent(test->sampler, test->now_ts, number, kRegularPacketSizeBytes, test->bytes_in_flight);
	test->bytes_in_flight += kRegularPacketSizeBytes;
}

static bbr_bandwidth_sample_t sample_tester_ack(sample_tester_t* test, int64_t number)
{
	size_t size = sampler_get_data_size(test->sampler, number);
	test->bytes_in_flight -= size;
	return sampler_on_packet_acked(test->sampler, test->now_ts, number);
}

static void sample_test_lost(sample_tester_t* test, int64_t number)
{
	size_t size = sampler_get_data_size(test->sampler, number);
	test->bytes_in_flight -= size;
	sampler_on_packet_lost(test->sampler, number);
}

static void send_40_packets_ack_20(sample_tester_t* test, int64_t delta_time)
{
	int i;

	for (i = 1; i <= 20; i++){
		sample_tester_send_packet(test, i);
		test->now_ts += delta_time;
	}

	for (i = 1; i <= 20; i++){
		sample_tester_ack(test, i);
		sample_tester_send_packet(test, i + 20);
		test->now_ts += delta_time;
	}
}

static void tester_send_and_wait()
{
	int delta_time = 10, i;
	int expected_bandwidth = kRegularPacketSizeBytes * 100 / 1000;
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	for (i = 1; i < 20; ++i){
		sample_tester_send_packet(&test, i);
		test.now_ts += delta_time;
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(expected_bandwidth, ret.bandwidth);
	}

	for (i = 20; i < 25; ++i){
		delta_time = delta_time * 2;
		expected_bandwidth = expected_bandwidth / 2;

		sample_tester_send_packet(&test, i);
		test.now_ts += delta_time;
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(expected_bandwidth, ret.bandwidth);
	}

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}

static void test_send_paced()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i;
	int expected_bandwidth = kRegularPacketSizeBytes / delta_time;

	send_40_packets_ack_20(&test, delta_time);

	for (i = 21; i <= 40; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(ret.bandwidth, expected_bandwidth);
		test.now_ts += delta_time;
	}

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}
/*¶ª°ü²âÊÔ*/
static void test_send_loss()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i;
	int expected_bandwidth = kRegularPacketSizeBytes / (delta_time * 2);
	
	for (i = 1; i <= 20; ++i){
		sample_tester_send_packet(&test, i);
		test.now_ts += delta_time;
	}

	for (i = 1; i <= 20; ++i){
		if (i % 2 == 0)
			sample_tester_ack(&test, i);
		else
			sample_test_lost(&test, i);
		sample_tester_send_packet(&test, i + 20);
		test.now_ts += delta_time;
	}

	for (i = 21; i <= 40; ++i){
		if (i % 2 == 0){
			ret = sample_tester_ack(&test, i);
			EXPECT_EQ(ret.bandwidth, expected_bandwidth);
		}
		else
			sample_test_lost(&test, i);

		test.now_ts += delta_time;
	}

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}
/*¶¶¶¯²âÊÔ*/
static void test_compress_ack()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i;
	int expected_bandwidth = kRegularPacketSizeBytes / delta_time;

	send_40_packets_ack_20(&test, delta_time);

	test.now_ts += delta_time * 15;
	for (i = 21; i <= 40; i++){
		ret = sample_tester_ack(&test, i);
	}

	EXPECT_EQ(ret.bandwidth, expected_bandwidth);

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}
/*ÂÒÐò²âÊÔ*/
static void test_reorder_ack()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i;
	int expected_bandwidth = kRegularPacketSizeBytes / delta_time;

	send_40_packets_ack_20(&test, delta_time);

	for (i = 0; i < 20; i++){
		sample_tester_ack(&test, 40 - i);
		sample_tester_send_packet(&test, 41 + i);
		test.now_ts += delta_time;
	}

	for (i = 41; i <= 60; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(ret.bandwidth, expected_bandwidth);
		test.now_ts += delta_time;
	}

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}

static void test_app_limited()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i;
	int expected_bandwidth = kRegularPacketSizeBytes / delta_time;

	send_40_packets_ack_20(&test, delta_time);

	/*ÉèÖÃapp limited*/
	sampler_on_app_limited(test.sampler);

	for (i = 21; i <= 40; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(ret.bandwidth, expected_bandwidth);
		test.now_ts += delta_time;
	}

	test.now_ts += 1000;

	for (i = 41; i <= 60; i++){
		sample_tester_send_packet(&test, i);
		test.now_ts += delta_time;
	}

	/*app limited!!!*/
	for (i = 41; i <= 60; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(ret.is_app_limited, 1);
		EXPECT_LT(ret.bandwidth, 0.7 * expected_bandwidth);

		sample_tester_send_packet(&test, i + 20);
		test.now_ts += delta_time;
	}

	for (i = 61; i <= 80; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_EQ(ret.bandwidth, expected_bandwidth);
		test.now_ts += delta_time;
	}

	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));
	EXPECT_EQ(0, test.bytes_in_flight);

	sample_tester_destroy(&test);
}

static void test_first_round_trip()
{
	bbr_bandwidth_sample_t ret;

	sample_tester_t test;
	sample_tester_init(&test);

	int delta_time = 1, i, last;
	int expected_bandwidth = kRegularPacketSizeBytes / delta_time;

	int64_t rtt = 800;
	int packets_of_num = 10;

	int real_bandwidth;

	real_bandwidth = packets_of_num * kRegularPacketSizeBytes / rtt;

	for (i = 1; i <= 10; i++){
		sample_tester_send_packet(&test, i);
		test.now_ts += delta_time;
	}

	test.now_ts += rtt - packets_of_num * delta_time;
	last = 0;
	for (i = 1; i <= 10; i++){
		ret = sample_tester_ack(&test, i);
		EXPECT_GE(ret.bandwidth, last);
		last = ret.bandwidth;
		test.now_ts += delta_time;
	}

	EXPECT_LT(last, real_bandwidth);
	EXPECT_GE(last, 0.9f * real_bandwidth);

	sample_tester_destroy(&test);
}

static void test_remove_olds()
{
	sample_tester_t test;
	sample_tester_init(&test);

	sample_tester_send_packet(&test, 1);
	sample_tester_send_packet(&test, 2);
	sample_tester_send_packet(&test, 3);
	sample_tester_send_packet(&test, 4);
	sample_tester_send_packet(&test, 5);

	test.now_ts += 100;

	EXPECT_EQ(5, sampler_get_sample_num(test.sampler));

	/**/
	sampler_remove_old(test.sampler, 4);
	EXPECT_EQ(2, sampler_get_sample_num(test.sampler));

	sampler_on_packet_lost(test.sampler, 4);
	EXPECT_EQ(1, sampler_get_sample_num(test.sampler));

	sample_tester_ack(&test, 5);
	EXPECT_EQ(0, sampler_get_sample_num(test.sampler));

	sample_tester_destroy(&test);
}

void test_bandwidth_sampler()
{
	tester_send_and_wait();
	test_send_paced();
	test_send_loss();
	test_compress_ack();
	test_reorder_ack();
	test_app_limited();
	test_first_round_trip();
	test_remove_olds();
}
