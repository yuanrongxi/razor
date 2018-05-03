/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "aimd_rate_control.h"
#include "razor_log.h"
#include <math.h>
#include <assert.h>

aimd_rate_controller_t* aimd_create(uint32_t max_rate, uint32_t min_rate)
{
	aimd_rate_controller_t* aimd = calloc(1, sizeof(aimd_rate_controller_t));
	aimd->max_rate = max_rate;
	aimd->min_rate = min_rate;

	aimd->curr_rate = 0;

	aimd->avg_max_bitrate_kbps = -1.0f;
	aimd->var_max_bitrate_kbps = 0.4f;
	aimd->state = kRcHold;
	aimd->region = kRcMaxUnknown;
	aimd->beta = 0.85f;
	aimd->time_last_bitrate_change = -1;
	aimd->time_first_incoming_estimate = -1;
	aimd->inited = -1;

	aimd->rtt = DEFAULT_RTT;
	aimd->in_experiment = 0;

	return aimd;
}

void aimd_destroy(aimd_rate_controller_t* aimd)
{
	if (aimd != NULL)
		free(aimd);
}

void aimd_set_start_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate)
{
	aimd->curr_rate = bitrate;
	aimd->inited = 0;
}

#define DEFAULT_FEELBACK_SIZE 64
#define MAX_FEELBACK_INTERVAL 1000
#define MIN_FELLBACK_INTERVAL 200
int64_t aimd_get_feelback_interval(aimd_rate_controller_t* aimd)
{
	/*计算feelback只占5%的带宽负荷下，发送feelback的时间间隔*/
	int64_t interval = (int64_t)(DEFAULT_FEELBACK_SIZE * 8 * 1000 / ((0.05 * aimd->curr_rate) + 0.5));
	interval = SU_MAX(SU_MIN(MAX_FEELBACK_INTERVAL, interval), MIN_FELLBACK_INTERVAL);

	return interval;
}

/*判断aimd控制器是否可以进行网络带宽调节*/
int aimd_time_reduce_further(aimd_rate_controller_t* aimd, int64_t cur_ts, uint32_t incoming_rate)
{
	int64_t reduce_interval = SU_MAX(SU_MIN(200, aimd->rtt), 10);

	if (cur_ts - aimd->time_last_bitrate_change >= reduce_interval)
		return 0;

	if (aimd->inited == 0 && aimd->curr_rate / 2 > incoming_rate)
		return 0;

	return -1;
}

void aimd_set_rtt(aimd_rate_controller_t* aimd, uint32_t rtt)
{
	aimd->rtt = rtt;
}

void aimd_set_min_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate)
{
	aimd->min_rate = bitrate;
	aimd->curr_rate = SU_MAX(aimd->min_rate, aimd->curr_rate);
}

void aimd_set_max_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate)
{
	aimd->max_rate = bitrate;
	aimd->curr_rate = SU_MIN(aimd->max_rate, aimd->curr_rate);
}

static uint32_t clamp_bitrate(aimd_rate_controller_t* aimd, uint32_t new_bitrate, uint32_t coming_rate)
{
	const uint32_t max_bitrate_bps = 3 * coming_rate / 2 + 10000;
	if (new_bitrate > aimd->curr_rate && new_bitrate > max_bitrate_bps)
		new_bitrate = SU_MAX(aimd->curr_rate, max_bitrate_bps);

	return SU_MIN(SU_MAX(new_bitrate, aimd->min_rate), aimd->max_rate);
}

/*计算一次带宽的增量,一般是用于初期增长阶段，有点象慢启动*/
static uint32_t multiplicative_rate_increase(int64_t cur_ts, int64_t last_ts, uint32_t curr_bitrate)
{
	double alpha = 1.08;
	uint32_t ts_since;

	if (last_ts > -1) {
		ts_since = SU_MIN((uint32_t)(cur_ts - last_ts), 1000);
		alpha = pow(alpha, ts_since / 1000.0);
	}

	return (uint32_t)(SU_MAX(curr_bitrate * (alpha - 1.0), 1000.0));
}

int aimd_get_near_max_inc_rate(aimd_rate_controller_t* aimd)
{
	double bits_per_frame = aimd->curr_rate / 30.0;
	double packets_per_frame = ceil(bits_per_frame / (8.0 * 1000.0));
	double avg_packet_size_bits = bits_per_frame / packets_per_frame;

	/*Approximate the over-use estimator delay to 100 ms*/
	const int64_t response_time = (aimd->rtt + 100) * 2;
	return (int)(SU_MAX(4000, (avg_packet_size_bits * 1000) / response_time));
}

/*计算一个在稳定期间的带宽增量*/
static uint32_t additive_rate_increase(aimd_rate_controller_t* aimd, int64_t cur_ts, int64_t last_ts)
{
	return (uint32_t)((cur_ts - last_ts) * aimd_get_near_max_inc_rate(aimd) / 1000.0);
}

static void update_max_bitrate_estimate(aimd_rate_controller_t* aimd, float incoming_bitrate_kps)
{
	const float alpha = 0.05f;
	if (aimd->avg_max_bitrate_kbps == -1.0f) 
		aimd->avg_max_bitrate_kbps = incoming_bitrate_kps;
	else
		aimd->avg_max_bitrate_kbps = (1 - alpha) * aimd->avg_max_bitrate_kbps + alpha * incoming_bitrate_kps;

	/* Estimate the max bit rate variance and normalize the variance with the average max bit rate.*/
	const float norm = SU_MAX(aimd->avg_max_bitrate_kbps, 1.0f);
	aimd->var_max_bitrate_kbps = (1 - alpha) * aimd->var_max_bitrate_kbps + alpha * (aimd->avg_max_bitrate_kbps - incoming_bitrate_kps) * (aimd->avg_max_bitrate_kbps - incoming_bitrate_kps) / norm;
	/*0.4 ~= 14 kbit/s at 500 kbit/s*/
	if (aimd->var_max_bitrate_kbps < 0.4f)
		aimd->var_max_bitrate_kbps = 0.4f;

	/* 2.5f ~= 35 kbit/s at 500 kbit/s*/
	if (aimd->var_max_bitrate_kbps > 2.5f) 
		aimd->var_max_bitrate_kbps = 2.5f;
}

static void aimd_change_region(aimd_rate_controller_t* aimd, int region)
{
	aimd->region = region;
}

static void aimd_change_state(aimd_rate_controller_t* aimd, rate_control_input_t* input, int64_t cur_ts)
{
	switch (input->state) {
	case kBwNormal:
		if (aimd->state == kRcHold) {
			aimd->time_last_bitrate_change = cur_ts;
			aimd->state = kRcIncrease;
		}
		break;
	case kBwOverusing:
		if (aimd->state != kRcDecrease)
			aimd->state = kRcDecrease;
		break;
	case kBwUnderusing:
		aimd->state = kRcHold;
		break;
	default:
		assert(0);
	}
}

static uint32_t aimd_change_bitrate(aimd_rate_controller_t* aimd, uint32_t new_bitrate, rate_control_input_t* input, int64_t cur_ts)
{
	float incoming_kbitrate, max_kbitrate;
	
	if (input->incoming_bitrate == 0)
		input->incoming_bitrate = aimd->curr_rate;

	if (aimd->inited == -1 && input->state == kBwOverusing)
		return aimd->curr_rate;

	aimd_change_state(aimd, input, cur_ts);

	incoming_kbitrate = input->incoming_bitrate / 1000.0f;
	max_kbitrate = sqrt(aimd->avg_max_bitrate_kbps * aimd->var_max_bitrate_kbps);

	switch (aimd->state){
	case kRcHold:
		break;

	case kRcIncrease:
		if (aimd->avg_max_bitrate_kbps >= 0 && incoming_kbitrate > aimd->avg_max_bitrate_kbps + 3 * max_kbitrate){ /*当前码率比平均最大码率大很多，进行倍数增*/
			aimd_change_region(aimd, kRcMaxUnknown);
			aimd->avg_max_bitrate_kbps = -1.0f;
		}

		if (aimd->region == kRcNearMax) /*进行加性增*/
			new_bitrate += additive_rate_increase(aimd, cur_ts, aimd->time_last_bitrate_change);
		else /*起始阶段，进行倍数性增*/
			new_bitrate += multiplicative_rate_increase(cur_ts, aimd->time_last_bitrate_change, new_bitrate);

		aimd->time_last_bitrate_change = cur_ts;
		razor_debug("aimd: kRcIncrease, new bitrate = %u, input->incoming_bitrate = %u, curr_bitrate = %u\n", new_bitrate, input->incoming_bitrate, aimd->curr_rate);

		break;

	case kRcDecrease:
		new_bitrate = (uint32_t)(aimd->beta * input->incoming_bitrate + 0.5);
		if (new_bitrate > aimd->curr_rate){
			if (aimd->region != kRcMaxUnknown)
				new_bitrate = (uint32_t)(aimd->avg_max_bitrate_kbps * 1000 * aimd->beta + 0.5f);

			new_bitrate = SU_MIN(new_bitrate, aimd->curr_rate);
		}
		
		razor_debug("aimd: RcDecrease, new bitrate = %u, input->incoming_bitrate = %u, curr_bitrate = %u\n", new_bitrate, input->incoming_bitrate, aimd->curr_rate);

		aimd_change_region(aimd, kRcNearMax);

		if (aimd->inited == 0 && input->incoming_bitrate < aimd->curr_rate)
			aimd->last_decrease = aimd->curr_rate - new_bitrate;

		if (incoming_kbitrate < aimd->avg_max_bitrate_kbps - 3 * max_kbitrate)
			aimd->avg_max_bitrate_kbps = -1.0f;

		aimd->inited = 0;
		update_max_bitrate_estimate(aimd, incoming_kbitrate);

		aimd->state = kRcHold;
		aimd->time_last_bitrate_change = cur_ts;
		break;

	default:
		assert(0);
	}

	return clamp_bitrate(aimd, new_bitrate, input->incoming_bitrate);
}

/*更新一次输入的带宽，进行aimd带宽调整*/
#define k_initialization_ts 5000
uint32_t aimd_update(aimd_rate_controller_t* aimd, rate_control_input_t* input, int64_t cur_ts)
{
	if (aimd->inited == -1){
		if (aimd->time_first_incoming_estimate < 0){ /*确定第一次update的时间戳*/
			if (input->incoming_bitrate > 0)
				aimd->time_first_incoming_estimate = cur_ts;
		}
		else if (cur_ts - aimd->time_first_incoming_estimate > k_initialization_ts && input->incoming_bitrate > 0){ /*5秒后进行将统计到的带宽作为初始化带宽*/
			aimd->curr_rate = input->incoming_bitrate;
			aimd->inited = 0;
		}
	}

	aimd->curr_rate = aimd_change_bitrate(aimd, aimd->curr_rate, input, cur_ts);
	return aimd->curr_rate;
}

void aimd_set_estimate(aimd_rate_controller_t* aimd, int bitrate, int64_t cur_ts)
{
	aimd->inited = 0;
	aimd->curr_rate = clamp_bitrate(aimd, bitrate, bitrate);
	aimd->time_last_bitrate_change = cur_ts;
}

int aimd_get_expected_bandwidth_period(aimd_rate_controller_t* aimd)
{
	int kMinPeriodMs = 2000;
	int kDefaultPeriodMs = 3000;
	int kMaxPeriodMs = 50000;

	int increase_rate = aimd_get_near_max_inc_rate(aimd);
	if (aimd->last_decrease == 0)
		return kDefaultPeriodMs;

	return SU_MIN(kMaxPeriodMs,
		SU_MAX(1000 * (int64_t)(aimd->last_decrease) / increase_rate, kMinPeriodMs));
}
