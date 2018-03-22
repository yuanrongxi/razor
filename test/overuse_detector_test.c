#include "cf_platform.h"
#include "inter_arrival.h"
#include "estimator_common.h"
#include "overuse_detector.h"
#include "kalman_filter.h"
#include "common_test.h"

typedef struct
{
	inter_arrival_t*	arrival;
	kalman_filter_t*	estimator;
	overuse_detector_t*	detector;

	int64_t				now_ms;
	int64_t				receive_time_ms;
	uint32_t			rtp_timestamp;
	random_t			r;
}over_tester_t;

#define kTimestampGroupLengthUs 5

static over_tester_t* over_create()
{
	over_tester_t* tester = calloc(1, sizeof(over_tester_t));
	
	tester->detector = overuse_create();
	tester->arrival = create_inter_arrival(0, kTimestampGroupLengthUs);
	tester->estimator = kalman_filter_create();

	tester->rtp_timestamp = 10;
	tester->receive_time_ms = 0;
	tester->now_ms = 0;
	cf_random(&tester->r, 123456789);

	return tester;
}

static void over_destroy(over_tester_t* tester)
{
	if (tester == NULL)
		return;

	if (tester->arrival != NULL){
		destroy_inter_arrival(tester->arrival);
		tester->arrival = NULL;
	}

	if (tester->detector != NULL){
		overuse_destroy(tester->detector);
		tester->detector = NULL;
	}

	if (tester->estimator != NULL){
		kalman_filter_destroy(tester->estimator);
		tester->estimator = NULL;
	}

	free(tester);
}

static void over_setup(over_tester_t* tester)
{
	if (tester != NULL){
		overuse_destroy(tester->detector);
		tester->detector = overuse_create();
	}
}
static void over_update_detector(over_tester_t* tester, uint32_t rtp_timestamp, int64_t receive_time_ms, size_t packet_size)
{
	uint32_t timestamp_delta;
	int64_t time_delta;
	int size_delta;

	if (inter_arrival_compute_deltas(tester->arrival, rtp_timestamp, receive_time_ms, receive_time_ms, packet_size, &timestamp_delta, &time_delta, &size_delta) == 0){
		kalman_filter_update(tester->estimator, time_delta, timestamp_delta, size_delta, tester->detector->state, receive_time_ms);

		overuse_detect(tester->detector, tester->estimator->offset, timestamp_delta, tester->estimator->num_of_deltas, receive_time_ms);
	}
}

static int run_100000_samples(over_tester_t* tester, int packets_per_frame, size_t packet_size, int mean_ms, int standard_deviation_ms)
{
	int unique_overuse = 0;
	int last_overuse = -1;
	int i, j;

	for (i = 0; i < 100000; ++i){
		for (j = 0; j < packets_per_frame; ++j){
			over_update_detector(tester, tester->rtp_timestamp, tester->receive_time_ms, packet_size);
		}

		tester->rtp_timestamp += mean_ms;
		tester->now_ms += mean_ms;

		tester->receive_time_ms = SU_MAX(tester->receive_time_ms, tester->now_ms + (int64_t)(cf_gaussian(&tester->r, 0, standard_deviation_ms) + 0.5));

		if (kBwOverusing == tester->detector->state){
			if (last_overuse + 1 != i)
				unique_overuse;

			last_overuse = i;
		}
	}

	return unique_overuse;
}

static int run_unit_overuse(over_tester_t* tester, int packets_per_frame, size_t packet_size, int mean_ms, int standard_deviation_ms, int drift_per_frame_ms)
{
	int i, j;

	for (i = 0; i < 1000; ++i){
		for (j = 0; j < packets_per_frame; j++)
			over_update_detector(tester, tester->rtp_timestamp, tester->receive_time_ms, packet_size);

		tester->rtp_timestamp += mean_ms;
		tester->now_ms += mean_ms + drift_per_frame_ms;
		tester->receive_time_ms = SU_MAX(tester->receive_time_ms, tester->now_ms + (int64_t)(cf_gaussian(&tester->r, 0, standard_deviation_ms) + 0.5));

		if (kBwOverusing == tester->detector->state)
			return i + 1;
	}

	return -1;
}

static void simple_non_overuse_30fps()
{
	size_t packet_size = 1200;
	uint32_t frame_duration_ms = 33;
	uint32_t rtp_timestamp = 10;
	int i;

	over_tester_t* tester = over_create();

	for (i = 0; i < 1000; ++i){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);

		rtp_timestamp += frame_duration_ms;
		tester->now_ms += frame_duration_ms;

		EXPECT_EQ(kBwNormal, tester->detector->state);
	}

	over_destroy(tester);
}

static void simple_non_overuse_with_reciver_var()
{
	uint32_t frame_duration_ms = 10;
	uint32_t rtp_timestamp = 10;
	size_t packet_size = 1200;
	int i;

	over_tester_t* tester = over_create();
	for (i = 0; i < 1000; ++i){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);

		rtp_timestamp += frame_duration_ms;
		if (i % 2 == 0)
			tester->now_ms += frame_duration_ms - 5;
		else
			tester->now_ms += frame_duration_ms + 5;
	}

	EXPECT_EQ(kBwNormal, tester->detector->state);

	over_destroy(tester);
}

static void simple_non_rtptimestamp_var()
{
	uint32_t frame_duration_ms = 10;
	uint32_t rtp_timestamp = 10;
	size_t packet_size = 1200;
	int i;

	over_tester_t* tester = over_create();
	for (i = 0; i < 1000; ++i){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);

		tester->now_ms += frame_duration_ms;
		if (i % 2 == 0)
			rtp_timestamp += frame_duration_ms - 5;
		else
			rtp_timestamp += frame_duration_ms + 5;
	}

	EXPECT_EQ(kBwNormal, tester->detector->state);

	over_destroy(tester);
}

static void simple_non_overuse_2000kbit30fps()
{  
	size_t packet_size = 1200;
	int packets_per_frame = 6;
	int frame_duration_ms = 33;
	int drift_per_frame_ms = 1;
	int sigma_ms = 0;  // No variance.
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();

	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size, frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);

	EXPECT_EQ(7, frames_until_overuse);

	over_destroy(tester);
}

static void simple_non_overuse_100kbit10fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 1;
	int frame_duration_ms = 100;
	int drift_per_frame_ms = 1;
	int sigma_ms = 0;  // No variance.
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();

	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);

	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(7, frames_until_overuse);

	over_destroy(tester);
}

static void overuse_with_high_var_100kps10fps()
{
	uint32_t frame_duration_ms = 100;
	uint32_t drift_per_frame_ms = 10;
	uint32_t rtp_timestamp = frame_duration_ms;
	size_t packet_size = 1200;
	int offset = 10;

	int i, j;

	over_tester_t* tester = over_create();
	for (i = 0; i < 1000; ++i){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		rtp_timestamp += frame_duration_ms;
		if (i % 2) {
			offset = cf_rand_region(&tester->r, 0, 49);
			tester->now_ms += frame_duration_ms - offset;
		}
		else 
			tester->now_ms += frame_duration_ms + offset;

		EXPECT_EQ(kBwNormal, tester->detector->state);
	}

	for (j = 0; j < 15; j++){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		tester->now_ms += frame_duration_ms + drift_per_frame_ms;
		rtp_timestamp += frame_duration_ms;
		EXPECT_EQ(kBwNormal, tester->detector->state);
	}

	over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
	EXPECT_EQ(kBwOverusing, tester->detector->state);

	over_destroy(tester);
}

static void overuse_with_low_var_2000Kbit30fps()
{
	uint32_t frame_duration_ms = 33;
	uint32_t drift_per_frame_ms = 1;
	uint32_t rtp_timestamp = frame_duration_ms;
	size_t packet_size = 1200;
	int offset = 0;

	int i, j;

	over_tester_t* tester = over_create();
	for (i = 0; i < 1000; i++){
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		rtp_timestamp += frame_duration_ms;
		if (i % 2) {
			offset = cf_rand_region(&tester->r, 0, 1);
			tester->now_ms += frame_duration_ms - offset;
		}
		else {
			tester->now_ms += frame_duration_ms + offset;
		}
		EXPECT_EQ(kBwNormal, tester->detector->state);
	}

	for (j = 0; j < 3; ++j) {
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
		tester->now_ms += frame_duration_ms + drift_per_frame_ms * 6;
		rtp_timestamp += frame_duration_ms;
		EXPECT_EQ(kBwNormal, tester->detector->state);
	}

	over_update_detector(tester, rtp_timestamp, tester->now_ms, packet_size);
	EXPECT_EQ(kBwOverusing, tester->detector->state);

	over_destroy(tester);
}

static void low_gaussian_var_30Kbit3fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 1;
	int frame_duration_ms = 333;
	int drift_per_frame_ms = 1;
	int sigma_ms = 3;
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();

	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(15, frames_until_overuse);

	over_destroy(tester);
}

static void low_gaussian_var_fast_drift30Kbit3fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 1;
	int frame_duration_ms = 333;
	int drift_per_frame_ms = 100;
	int sigma_ms = 3;
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();

	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(4, frames_until_overuse);

	over_destroy(tester);
}

static void HighGaussianVariance30Kbit3fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 1;
	int frame_duration_ms = 333;
	int drift_per_frame_ms = 1;
	int sigma_ms = 10;

	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();
	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(50, frames_until_overuse);

	over_destroy(tester);
}

static void HighGaussianVarianceFastDrift30Kbit3fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 1;
	int frame_duration_ms = 333;
	int drift_per_frame_ms = 100;
	int sigma_ms = 10;
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();
	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(4, frames_until_overuse);

	over_destroy(tester);
}

static void MAYBE_LowGaussianVariance1000Kbit30fps()
{
	size_t packet_size = 1200;
	int packets_per_frame = 3;
	int frame_duration_ms = 33;
	int drift_per_frame_ms = 1;
	int sigma_ms = 3;
	int unique_overuse, frames_until_overuse;

	over_tester_t* tester = over_create();
	unique_overuse = run_100000_samples(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms);
	EXPECT_EQ(0, unique_overuse);

	frames_until_overuse = run_unit_overuse(tester, packets_per_frame, packet_size,
		frame_duration_ms, sigma_ms, drift_per_frame_ms);
	EXPECT_EQ(15, frames_until_overuse);

	over_destroy(tester);
}

void test_detector()
{
	simple_non_overuse_30fps();
	simple_non_overuse_with_reciver_var();
	simple_non_rtptimestamp_var();

	simple_non_overuse_2000kbit30fps();
	simple_non_overuse_100kbit10fps();

	overuse_with_high_var_100kps10fps();
	overuse_with_low_var_2000Kbit30fps();

	low_gaussian_var_30Kbit3fps();
	low_gaussian_var_fast_drift30Kbit3fps();
	HighGaussianVariance30Kbit3fps();
	HighGaussianVarianceFastDrift30Kbit3fps();

	MAYBE_LowGaussianVariance1000Kbit30fps();
}


