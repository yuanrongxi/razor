#include "common_test.h"
#include "sender_history.h"

#define k_limited_ms 1000

static void add_remove_one()
{
	uint16_t seq_no = 16;

	packet_feedback_t k_packet;
	packet_feedback_t received_packet, received_packet2;

	sender_history_t* hist = sender_history_create(k_limited_ms);

	init_packet_feedback(k_packet);
	k_packet.sequence_number = seq_no;
	k_packet.create_ts = k_packet.send_ts = GET_SYS_MS();
	k_packet.payload_size = 0;

	sender_history_add(hist, &k_packet);

	init_packet_feedback(received_packet);
	assert(sender_history_get(hist, seq_no, &received_packet, 1) == 0);
	EXPECT_EQ(k_packet.create_ts, received_packet.create_ts, 1);

	init_packet_feedback(received_packet2);
	assert(sender_history_get(hist, seq_no, &received_packet2, 1) != 0);

	sender_history_destroy(hist);
}

static void populates_expected_fields()
{
	const uint16_t kSeqNo = 10;
	const int64_t kSendTime = 1000;
	const int64_t kReceiveTime = 2000;
	const size_t kPayloadSize = 42;
	packet_feedback_t k_packet, recvd_packet;

	sender_history_t* hist = sender_history_create(k_limited_ms);

	init_packet_feedback(k_packet);
	k_packet.sequence_number = kSeqNo;
	k_packet.payload_size = kPayloadSize;
	k_packet.send_ts = kSendTime;
	sender_history_add(hist, &k_packet);

	init_packet_feedback(recvd_packet);
	recvd_packet.sequence_number = kSeqNo;
	assert(sender_history_get(hist, kSeqNo, &recvd_packet,1) == 0);
	recvd_packet.arrival_ts = kReceiveTime;

	EXPECT_EQ(kReceiveTime, recvd_packet.arrival_ts);
	EXPECT_EQ(kSendTime, recvd_packet.send_ts);
	EXPECT_EQ(kSeqNo, recvd_packet.sequence_number);
	EXPECT_EQ(kPayloadSize, recvd_packet.payload_size);

	sender_history_destroy(hist);
}

static void test_outstanding_bytes()
{
	const uint16_t kSeqNo = 10;
	const int64_t kSendTime = 1000;
	const int64_t kReceiveTime = 2000;
	const size_t kPayloadSize = 42;

	sender_history_t* hist = sender_history_create(k_limited_ms);
	int i;
	packet_feedback_t k_packet, recvd_packet;

	for (i = 0; i < 10; i++){
		init_packet_feedback(k_packet);
		k_packet.sequence_number = kSeqNo + i;
		k_packet.payload_size = kPayloadSize;
		k_packet.send_ts = kSendTime + i * 20;
		k_packet.create_ts = k_packet.send_ts;
		sender_history_add(hist, &k_packet);
	}

	EXPECT_EQ(kPayloadSize * 10, sender_history_outstanding_bytes(hist));

	init_packet_feedback(recvd_packet);
	recvd_packet.sequence_number = kSeqNo + 4;
	assert(sender_history_get(hist, kSeqNo + 4, &recvd_packet, 1) == 0);
	recvd_packet.arrival_ts = kReceiveTime;

	EXPECT_EQ(kPayloadSize * 5, sender_history_outstanding_bytes(hist));

	init_packet_feedback(k_packet);
	k_packet.sequence_number = kSeqNo + 10;
	k_packet.payload_size = kPayloadSize;
	k_packet.send_ts = kSendTime + k_limited_ms + 200;
	k_packet.create_ts = k_packet.send_ts;
	sender_history_add(hist, &k_packet);

	EXPECT_EQ(kPayloadSize * 1, sender_history_outstanding_bytes(hist));

	sender_history_destroy(hist);
}


void test_sender_history()
{
	add_remove_one();
	populates_expected_fields();
	test_outstanding_bytes();
}


