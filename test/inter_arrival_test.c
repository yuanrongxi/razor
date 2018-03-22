#include "inter_arrival.h"
#include "cf_platform.h"
#include "common_test.h"
#include <assert.h>

#define kTimestampGroupLengthUs 5
#define kMinStep 20
#define kTriggerNewGroupUs (kTimestampGroupLengthUs + 1)
#define kBurstThresholdMs 5

static void expect_false(inter_arrival_t* arr, uint32_t ts, int64_t arrival_ts, size_t packet_size)
{
	uint32_t dummy_timestamp = 101;
	int64_t dummy_arrival_time_ms = 303;
	int dummy_packet_size = 909;

	int computed = inter_arrival_compute_deltas(arr, ts, arrival_ts, arrival_ts, packet_size, &dummy_timestamp, &dummy_arrival_time_ms, &dummy_packet_size);

	assert(computed == -1);
	assert(101 == dummy_timestamp);
	assert(303 == dummy_arrival_time_ms);
	assert(909 == dummy_packet_size);
}

static void expect_true(inter_arrival_t* arr, uint32_t ts, int64_t arrival_ts, size_t packet_size, 
						int64_t expected_ts_delta, int64_t expected_arrival_delta, int expected_size_delta, uint32_t ts_near)
{
	uint32_t delta_timestamp = 101;
	int64_t delta_arrival_time_ms = 303;
	int delta_packet_size = 909;

	int computed = inter_arrival_compute_deltas(arr, ts, arrival_ts, arrival_ts, packet_size, &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);

	assert(computed == 0);
	EXPECT_NEAR(expected_ts_delta, delta_timestamp, ts_near);
	EXPECT_EQ(delta_arrival_time_ms, expected_arrival_delta);
	EXPECT_EQ(delta_packet_size, expected_size_delta);
}

static void wrap_test_helper(inter_arrival_t* arr, uint32_t wrap_start_ts, uint32_t near_ts, int unorderly_within_group)
{
	int i;

	/*G1*/
	int64_t arrival_time = 17, g4_arrival_time, g5_arrival_time, g6_arrival_time;
	expect_false(arr, 0, arrival_time, 1);

	/*G2*/
	arrival_time += kBurstThresholdMs + 1;
	expect_false(arr, wrap_start_ts / 4, arrival_time, 1);

	/*G3*/
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, wrap_start_ts / 2, arrival_time, 1, wrap_start_ts / 4, 6, 0, near_ts);

	/*G4*/
	arrival_time += kBurstThresholdMs + 1;
	g4_arrival_time = arrival_time;
	expect_true(arr, wrap_start_ts / 2 + wrap_start_ts / 4, arrival_time, 1, wrap_start_ts / 4, 6, 0, near_ts);

	/*G5*/
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, wrap_start_ts, arrival_time, 2, wrap_start_ts / 4, 6, 0, near_ts);
	for (i = 0; i < 10; i++){
		arrival_time += kBurstThresholdMs + 1;
		if (unorderly_within_group == 0) /*½Ó½üÍ¬Ò»Ê±¿ÌÂÒÐò²âÊÔ*/
			expect_false(arr, wrap_start_ts + (kMinStep * (9 - i)) / 1000, arrival_time, 1);
		else
			expect_false(arr, wrap_start_ts + (kMinStep * i) / 1000, arrival_time, 1);
	}

	g5_arrival_time = arrival_time;
	/*ÂÒÐò²âÊÔ*/
	arrival_time += kBurstThresholdMs + 1;
	expect_false(arr, wrap_start_ts - 1, arrival_time, 100);

	/*G6*/
	arrival_time += kBurstThresholdMs + 1;
	g6_arrival_time = arrival_time;
	expect_true(arr, wrap_start_ts + kTriggerNewGroupUs, arrival_time, 10,
		wrap_start_ts / 4 + (9 * kMinStep) / 1000,
		g5_arrival_time - g4_arrival_time, (2 + 10) - 1, near_ts);

	arrival_time += kBurstThresholdMs + 1;
	expect_false(arr, wrap_start_ts + kTimestampGroupLengthUs, arrival_time, 100);

	/*G7*/
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, wrap_start_ts + 2 * (kTriggerNewGroupUs),
		arrival_time, 100, kTriggerNewGroupUs,
		g6_arrival_time - g5_arrival_time, 10 - (2 + 10), near_ts);
}

#define kStartRtpTimestampWrapMs 3400

static void first_packet()
{
	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);

	expect_false(arr, 0, 17, 1);

	destroy_inter_arrival(arr);
}

static void first_group()
{
	int64_t arrival_time = 17;
	int64_t g1_arrival_time = arrival_time, g2_arrival_time;

	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);

	/*G1*/
	expect_false(arr, 0, arrival_time, 1);

	/*G2*/
	arrival_time += kBurstThresholdMs + 1;
	g2_arrival_time = arrival_time;
	expect_false(arr, kTriggerNewGroupUs, arrival_time, 2);

	/*G3*/
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, 2 * kTriggerNewGroupUs, arrival_time, 1, kTriggerNewGroupUs, g2_arrival_time - g1_arrival_time, 1, 0);

	destroy_inter_arrival(arr);
}

static void second_group()
{
	int64_t arrival_time = 17;
	int64_t g1_arrival_time = arrival_time, g2_arrival_time, g3_arrival_time;

	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);

	/*G1*/
	expect_false(arr, 0, arrival_time, 1);

	/*G2*/
	arrival_time += kBurstThresholdMs + 1;
	g2_arrival_time = arrival_time;
	expect_false(arr, kTriggerNewGroupUs, arrival_time, 2);

	/*G3*/
	arrival_time += kBurstThresholdMs + 1;
	g3_arrival_time = arrival_time;
	expect_true(arr, 2 * kTriggerNewGroupUs, arrival_time, 1, kTriggerNewGroupUs, g2_arrival_time - g1_arrival_time, 1, 0);

	/*G4, First packet of 4th group yields deltas between group 2 and 3*/
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, 3 * kTriggerNewGroupUs, arrival_time, 2, kTriggerNewGroupUs, g3_arrival_time - g2_arrival_time, -1, 0);

	destroy_inter_arrival(arr);
}

static void aacumulated_group()
{
	int64_t arrival_time = 17;
	int64_t g1_arrival_time, timestamp;
	int64_t g2_arrival_time, g2_timestamp;
	int i;

	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);

	/*G1*/
	g1_arrival_time = arrival_time;
	expect_false(arr, 0, arrival_time, 1);

	/*G2*/
	arrival_time += kBurstThresholdMs + 1;
	expect_false(arr, kTriggerNewGroupUs, 28, 2);
	timestamp = kTriggerNewGroupUs;
	for (i = 0; i < 10; i++){
		arrival_time += kBurstThresholdMs + 1;
		timestamp += kMinStep / 1000;
		expect_false(arr, timestamp, arrival_time, 1);
	}

	g2_arrival_time = arrival_time;
	g2_timestamp = timestamp;

	// G3
	arrival_time = 500;
	expect_true(arr, 2 * kTriggerNewGroupUs, arrival_time, 100,
		g2_timestamp, g2_arrival_time - g1_arrival_time, (2 + 10) - 1,   0);

	destroy_inter_arrival(arr);
}

static void two_bursts()
{
	int i;
	int64_t g1_arrival_time, timestamp, arrival_time, g2_arrival_time, g2_timestamp;

	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);

	/*G1*/
	g1_arrival_time = 17;
	expect_false(arr, 0, g1_arrival_time, 1);

	/*G2*/
	timestamp = kTriggerNewGroupUs;
	arrival_time = 100;
	for (i = 0; i < 10; i++){
		timestamp += 30;
		arrival_time += kBurstThresholdMs;
		expect_false(arr, timestamp, arrival_time, 1);
	}

	g2_arrival_time = arrival_time;
	g2_timestamp = timestamp;

	/*G3*/
	timestamp += 30;
	arrival_time += kBurstThresholdMs + 1;
	expect_true(arr, timestamp, arrival_time, 100,
		g2_timestamp, g2_arrival_time - g1_arrival_time,
		10 - 1,  // Delta G2-G1
		0);

	destroy_inter_arrival(arr);
}

static void test_start_timestmap_wap()
{
	inter_arrival_t* arr;
	arr = create_inter_arrival(0, kTimestampGroupLengthUs);
	wrap_test_helper(arr, kStartRtpTimestampWrapMs, 1, -1);

	destroy_inter_arrival(arr);
}

void test_inter_arrival()
{
	first_packet();
	first_group();
	aacumulated_group();

	two_bursts();

	test_start_timestmap_wap();
}


