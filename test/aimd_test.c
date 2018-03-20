#include "aimd_rate_control.h"
#include <assert.h>

static int kMinBwePeriodMs = 2000;
static int kMaxBwePeriodMs = 50000;
static int kDefaultPeriodMs = 3000;

static double kFractionAfterOveruse = 0.85;

static void update_rate_control(aimd_rate_controller_t* aimd, int state, int bitrate, int64_t cur_ts)
{
	rate_control_input_t input;
	
	input.incoming_bitrate = bitrate;
	input.noise_var = cur_ts;
	input.state = state;

	aimd_update(aimd, &input, cur_ts);
}

#define aimd_max_rate 30000000
#define aimd_min_rate 10000

static void min_near_increate_rate_on_low_bandwidth()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	aimd_set_estimate(aimd, 30000, GET_SYS_MS());

	assert(aimd_get_near_max_inc_rate(aimd) == 4000);

	aimd_destroy(aimd);
}

static void near_max_increate_200ms_rtt()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	aimd_set_estimate(aimd, 90000, GET_SYS_MS());
	aimd_set_rtt(aimd, 200);

	assert(aimd_get_near_max_inc_rate(aimd) == 5000);

	aimd_destroy(aimd);
}

static void near_max_increate_100ms_rtt()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	aimd_set_estimate(aimd, 60000, GET_SYS_MS());
	
	aimd_set_rtt(aimd, 100);

	assert(aimd_get_near_max_inc_rate(aimd) == 5000);

	aimd_destroy(aimd);
}

static inline near_expect(uint32_t src, uint32_t dst, uint32_t delta)
{
	assert(dst <= src + delta && dst >= src - delta);
}

static void get_increate_rate_period()
{
	uint32_t near_rate;
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);

	aimd_set_estimate(aimd, 300000, GET_SYS_MS());
	update_rate_control(aimd, kBwOverusing, 300000, GET_SYS_MS());

	near_rate = aimd_get_near_max_inc_rate(aimd);
	/*near_expect(14000, near_rate, 1000);*/

	assert(3000 == aimd_get_expected_bandwidth_period(aimd));

	aimd_destroy(aimd);
}

static void bwe_limited_acked_bitrate()
{
	int acked_bitrate = 10000;
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	int64_t cur_ts, start_ts;
	start_ts = cur_ts = GET_SYS_MS();

	aimd_set_estimate(aimd, acked_bitrate, cur_ts);
	while (cur_ts - start_ts < 20000){
		update_rate_control(aimd, kBwNormal, acked_bitrate, cur_ts);
		cur_ts += 100;
	}

	assert(aimd->inited == 0);
	if (3 * acked_bitrate / 2 + 10000 != aimd->curr_rate)
		assert(0);

	aimd_destroy(aimd);
}

static void bwe_not_limited_decreasing_acked_bitrate()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	uint32_t acked_bitrate = 100000, prev_estimate, new_bitrate;
	int64_t cur_ts, start_ts;
	
	start_ts = cur_ts = GET_SYS_MS();

	aimd_set_estimate(aimd, acked_bitrate, cur_ts);
	while (cur_ts - start_ts < 20000){
		update_rate_control(aimd, kBwNormal, acked_bitrate, cur_ts);
		cur_ts += 100;
	}

	assert(aimd->inited == 0);
	prev_estimate = aimd->curr_rate;

	update_rate_control(aimd, kBwNormal, acked_bitrate / 2, cur_ts);

	new_bitrate = aimd->curr_rate;

	near_expect(new_bitrate, 3 * acked_bitrate / 2 + 10000, 2000);
	assert(prev_estimate == new_bitrate);

	aimd_destroy(aimd);
}

static void expected_period_after_20kbps_increase()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	uint32_t inited_bitrate = 110000, acked_bitrate;
	int64_t cur_ts;

	cur_ts = GET_SYS_MS();

	aimd_set_estimate(aimd, inited_bitrate, cur_ts);
	cur_ts += 100;

	acked_bitrate = (inited_bitrate - 20000) / kFractionAfterOveruse;
	update_rate_control(aimd, kBwOverusing, acked_bitrate, cur_ts);

	assert(5000 == aimd_get_near_max_inc_rate(aimd));
	assert(4000 == aimd_get_expected_bandwidth_period(aimd));

	aimd_destroy(aimd);
}

static void bandwidth_period_not_below_min()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	uint32_t inited_bitrate = 10000;
	int64_t cur_ts;

	cur_ts = GET_SYS_MS();
	aimd_set_estimate(aimd, inited_bitrate, cur_ts);
	cur_ts += 100;

	update_rate_control(aimd, kBwOverusing, inited_bitrate - 1, cur_ts);
	assert(kMinBwePeriodMs == aimd_get_expected_bandwidth_period(aimd));

	aimd_destroy(aimd);
}

static void bandwidth_period_not_above_max()
{
	aimd_rate_controller_t* aimd = aimd_create(aimd_max_rate, aimd_min_rate);
	uint32_t inited_bitrate = 10010000;
	int64_t cur_ts;

	cur_ts = GET_SYS_MS();
	aimd_set_estimate(aimd, inited_bitrate, cur_ts);
	cur_ts += 100;

	update_rate_control(aimd, kBwOverusing, 10000 / kFractionAfterOveruse, cur_ts);

	assert(kMaxBwePeriodMs == aimd_get_expected_bandwidth_period(aimd));

	aimd_destroy(aimd);
}

void test_aimd()
{
	min_near_increate_rate_on_low_bandwidth();

	near_max_increate_200ms_rtt();
	near_max_increate_100ms_rtt();

	get_increate_rate_period();

	bwe_limited_acked_bitrate();

	bwe_not_limited_decreasing_acked_bitrate();

	expected_period_after_20kbps_increase();

	bandwidth_period_not_below_min();
	bandwidth_period_not_above_max();
}


