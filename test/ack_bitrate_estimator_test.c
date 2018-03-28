#include "ack_bitrate_estimator.h"
#include "common_test.h"
#include <stdio.h>

#define k_arrival_ms	10
#define k_send_ms		10
#define	k_payload_size  100


static void create_feedback_packets(packet_feedback_t* packets, int size)
{
	int i;

	for (i = 0; i < size; i++){
		packets[i].sequence_number = i;
		packets[i].create_ts = 0;
		packets[i].send_ts = i * k_send_ms + k_send_ms;
		packets[i].arrival_ts = i * k_arrival_ms + k_arrival_ms;
		packets[i].payload_size = k_payload_size;
	}
}

#define k_packet_num 120
static void estimate_ack_bitrate()
{
	ack_bitrate_estimator_t* est;
	packet_feedback_t packets[k_packet_num];
	uint32_t bitrate;

	est = ack_estimator_create();

	create_feedback_packets(packets, k_packet_num);
	ack_estimator_incoming(est, packets, k_packet_num);

	bitrate = ack_estimator_bitrate_bps(est);

	EXPECT_EQ(80000, bitrate);

	ack_estimator_destroy(est);
}

static void estimate_ack_alr_bitrate()
{
	ack_bitrate_estimator_t* est;
	packet_feedback_t packets[k_packet_num];
	uint32_t bitrate;

	est = ack_estimator_create();

	create_feedback_packets(packets, k_packet_num);
	ack_estimator_set_alrended(est, 100 * k_arrival_ms + k_arrival_ms + 1);
	ack_estimator_incoming(est, packets, k_packet_num);

	bitrate = ack_estimator_bitrate_bps(est);

	EXPECT_EQ(80000, bitrate);

	ack_estimator_destroy(est);
}

void test_ack_bitrate_estimator()
{
	estimate_ack_bitrate();
	estimate_ack_alr_bitrate();
}


