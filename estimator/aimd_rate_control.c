#include "aimd_rate_control.h"
#include <math.h>
#include <assert.h>

aimd_rate_controller_t* aimd_create(uint32_t max_rate, uint32_t min_rate)
{
	aimd_rate_controller_t* aimd = calloc(1, sizeof(aimd_rate_controller_t));
	aimd->max_rate = max_rate;
	aimd->min_rate = min_rate;

	aimd->curr_rate = 10 * 1024;

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
	/*����feelbackֻռ5%�Ĵ��������£�����feelback��ʱ����*/
	int64_t interval = DEFAULT_FEELBACK_SIZE * 8 * 1000 / ((0.05 * aimd->curr_rate) + 0.5);
	interval = SU_MAX(SU_MIN(MAX_FEELBACK_INTERVAL, interval), MIN_FELLBACK_INTERVAL);

	return interval;
}

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

static uint32_t clamp_bitrate(aimd_rate_controller_t* aimd, uint32_t new_bitrate, uint32_t coming_rate)
{
	const uint32_t max_bitrate_bps = 3 * coming_rate / 2 + 10000;
	if (new_bitrate > aimd->curr_rate && new_bitrate > max_bitrate_bps)
		new_bitrate = SU_MAX(aimd->curr_rate, max_bitrate_bps);

	return SU_MAX(new_bitrate, aimd->min_rate);
}

/*����һ�δ���������,һ�������ڳ��������׶Σ��е���������*/
static uint32_t multiplicative_rate_increase(int64_t cur_ts, int64_t last_ts, uint32_t curr_bitrate)
{
	double alpha = 1.08;
	uint32_t ts_since;

	if (last_ts > -1) {
		ts_since = SU_MIN(cur_ts - last_ts, 1000);
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

/*����һ�����ȶ��ڼ�Ĵ�������*/
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
		if (aimd->avg_max_bitrate_kbps >= 0 && incoming_kbitrate > aimd->avg_max_bitrate_kbps + 3 * max_kbitrate){ /*��ǰ���ʱ�ƽ��������ʴ�ܶ࣬���б�����*/
			aimd_change_region(aimd, kRcMaxUnknown);
			aimd->avg_max_bitrate_kbps = -1.0f;
		}

		if (aimd->region == kRcNearMax) /*���м�����*/
			new_bitrate += additive_rate_increase(aimd, cur_ts, aimd->time_last_bitrate_change);
		else /*��ʼ�׶Σ����б�������*/
			new_bitrate += multiplicative_rate_increase(cur_ts, aimd->time_last_bitrate_change, new_bitrate);

		aimd->time_last_bitrate_change = cur_ts;

		break;

	case kRcDecrease:
		new_bitrate = aimd->beta * input->incoming_bitrate + 0.5;
		if (new_bitrate > aimd->curr_rate){
			if (aimd->region != kRcMaxUnknown)
				new_bitrate = (uint32_t)(aimd->avg_max_bitrate_kbps * 1000 * aimd->beta + 0.5f);

			new_bitrate = SU_MIN(new_bitrate, aimd->curr_rate);
		}
		
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

/*����һ������Ĵ���������aimd��������*/
uint32_t aimd_update(aimd_rate_controller_t* aimd, rate_control_input_t* input, int64_t cur_ts)
{
	if (aimd->inited == -1){
		int64_t kInitializationTimeMs = 5000;

		if (aimd->time_first_incoming_estimate < 0){ /*ȷ����һ��update��ʱ���*/
			if (input->incoming_bitrate)
				aimd->time_first_incoming_estimate = cur_ts;
		}
		else if (cur_ts - aimd->time_first_incoming_estimate > kInitializationTimeMs && input->incoming_bitrate > 0){ /*5�����н�ͳ�Ƶ��Ĵ�����Ϊ��ʼ������*/
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