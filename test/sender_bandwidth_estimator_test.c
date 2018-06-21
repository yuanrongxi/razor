#include "sender_bandwidth_estimator.h"
#include "cf_platform.h"
#include "common_test.h"

/*对remb和delay base做测试*/
static void test_probing(int use_delay_based)
{
	sender_estimation_t* est;

	int remb_bps = 1000000;
	int second_remb_bps = remb_bps + 500000;
	int64_t now_ms = 0;

	uint32_t bitrate;
	uint8_t fraction_loss;
	int64_t rtt;

	est = sender_estimation_create(100000, 1500000);

	sender_estimation_set_send_bitrate(est, 200000);

	sender_estimation_update_block(est, 0, 50, 1, now_ms, 200000);

	if (use_delay_based == 0)
		sender_estimation_update_delay_base(est, now_ms, remb_bps, 0);
	else
		sender_estimation_update_remb(est, now_ms, remb_bps);
	sender_estimation_update(est, now_ms, 200000);

	rtt = est->last_rtt;
	bitrate = est->curr_bitrate;
	fraction_loss = est->last_fraction_loss;

	EXPECT_EQ(remb_bps, bitrate);

	now_ms += 2001;

	if (use_delay_based == 0)
		sender_estimation_update_delay_base(est, now_ms, second_remb_bps, 0);
	else
		sender_estimation_update_remb(est, now_ms, second_remb_bps);
	sender_estimation_update(est, now_ms, remb_bps);

	bitrate = 0;

	rtt = est->last_rtt;
	bitrate = est->curr_bitrate;
	fraction_loss = est->last_fraction_loss;

	EXPECT_EQ(remb_bps, bitrate);

	sender_estimation_destroy(est);
}

static void bitrate_decrease_without_following_remb()
{
	int k_min_bitrate_bps = 100000;
	int k_initial_bitrate_bps = 1000000;

	sender_estimation_t* est;

	uint8_t k_fraction_loss = 128;
	int64_t k_rtt = 50;

	int64_t now_ms = 0;
	int last_bitrate_bps;

	est = sender_estimation_create(k_min_bitrate_bps, 1500000);

	sender_estimation_set_send_bitrate(est, k_initial_bitrate_bps);

	EXPECT_EQ(est->curr_bitrate, k_initial_bitrate_bps);
	EXPECT_EQ(0, est->last_fraction_loss);
	EXPECT_EQ(0, est->last_rtt);

	/*模拟report事件,发生丢包了*/
	sender_estimation_update_block(est, k_fraction_loss, k_rtt, 100, now_ms, k_initial_bitrate_bps);
	now_ms += 1000;
	sender_estimation_update(est, now_ms, k_initial_bitrate_bps);

	EXPECT_LT(est->curr_bitrate, k_initial_bitrate_bps);
	EXPECT_EQ(est->last_fraction_loss, k_fraction_loss);
	EXPECT_EQ(est->last_rtt, k_rtt);

	last_bitrate_bps = est->curr_bitrate;

	now_ms += 1000;
	sender_estimation_update(est, now_ms, k_initial_bitrate_bps);

	EXPECT_EQ(est->curr_bitrate, last_bitrate_bps);
	EXPECT_EQ(est->last_fraction_loss, k_fraction_loss);
	EXPECT_EQ(est->last_rtt, k_rtt);

	sender_estimation_destroy(est);
}

 
void test_sender_bandwidth_estimator()
{
	test_probing(0);
	test_probing(1);

	bitrate_decrease_without_following_remb();
}




