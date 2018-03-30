#include "delay_base_bwe.h"
#include "ack_bitrate_estimator.h"
#include "cf_platform.h"
#include "estimator_common.h"
#include "common_test.h"

#define kNumProbesCluster0 5
#define kNumProbesCluster1 8

static void no_crash_empty_feedback()
{
	delay_base_bwe_t* bwe;
	packet_feedback_t packets[1];

	bwe = delay_bwe_create();

	delay_bwe_incoming(bwe, packets, 0, 10000, GET_SYS_MS());

	delay_bwe_destroy(bwe);
}

static void no_crash_only_lost_feedback()
{
	delay_base_bwe_t* bwe;
	packet_feedback_t packets[2];

	packets[0].arrival_ts = -1;
	packets[0].create_ts = -1;
	packets[0].send_ts = -1;
	packets[0].payload_size = 1500;
	packets[0].sequence_number = 0;

	packets[1].arrival_ts = -1;
	packets[1].create_ts = -1;
	packets[1].send_ts = -1;
	packets[1].payload_size = 1500;
	packets[1].sequence_number = 0;

	bwe = delay_bwe_create();

	delay_bwe_incoming(bwe, packets, 2, 10000, GET_SYS_MS());

	delay_bwe_destroy(bwe);
}

static bwe_result_t incoming_feedback_test(delay_base_bwe_t* bwe, ack_bitrate_estimator_t* acker, int64_t now_ts, uint16_t seq, int size)
{
	packet_feedback_t packet[1];
	
	packet[0].arrival_ts = now_ts;
	packet[0].create_ts = now_ts;
	packet[0].send_ts = now_ts;
	packet[0].sequence_number = seq;
	packet[0].payload_size = size;

	ack_estimator_incoming(acker, packet, 1);

	return delay_bwe_incoming(bwe, packet, 1, ack_estimator_bitrate_bps(acker), now_ts);

}

static void probe_detection()
{
	int64_t now_ts = 0;
	uint16_t seq_num = 0;
	int i;

	delay_base_bwe_t* bwe;
	ack_bitrate_estimator_t* est;

	bwe_result_t result;

	bwe = delay_bwe_create();
	est = ack_estimator_create();

	for (i = 0; i < kNumProbesCluster0; ++i){
		now_ts += 10;
		result = incoming_feedback_test(bwe, est, now_ts, seq_num++, 1000);
	}

	delay_bwe_set_start_bitrate(bwe, 800 * 1024);

	for (i = 0; i < kNumProbesCluster1; ++i){
		now_ts += 5;
		result = incoming_feedback_test(bwe, est, now_ts, seq_num++, 1000);
	}

	assert(result.updated == 0);

	ack_estimator_destroy(est);
	delay_bwe_destroy(bwe);
}

void test_delay_base_bwe()
{
	no_crash_empty_feedback();
	no_crash_only_lost_feedback();

	probe_detection();
}
