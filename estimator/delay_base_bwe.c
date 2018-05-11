/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "delay_base_bwe.h"
#include "razor_log.h"

#define k_trendline_smoothing_coeff		0.9
#define k_trendline_threshold_gain		4.0
#define k_trendline_window_size			20

#define k_group_time					5
#define k_max_failed_count				5

#define k_min_bitrate					64000	  /*8KB/S*/
#define k_max_bitrate					12800000  /*1.6MB/S*/

#define k_timestamp_ms					2000

delay_base_bwe_t* delay_bwe_create()
{
	delay_base_bwe_t* bwe = calloc(1, sizeof(delay_base_bwe_t));
	bwe->last_seen_ms = -1;
	bwe->first_ts = GET_SYS_MS();

	bwe->trendline_window_size = k_trendline_window_size;
	bwe->trendline_smoothing_coeff = k_trendline_smoothing_coeff;
	bwe->trendline_threshold_gain = k_trendline_threshold_gain;

	bwe->rate_control = aimd_create(k_max_bitrate, k_min_bitrate);
	bwe->detector = overuse_create();

	return bwe;
}

void delay_bwe_destroy(delay_base_bwe_t* bwe)
{
	if (bwe == NULL)
		return;

	if (bwe->rate_control != NULL){
		aimd_destroy(bwe->rate_control);
		bwe->rate_control = NULL;
	}

	if (bwe->detector != NULL){
		overuse_destroy(bwe->detector);
		bwe->detector = NULL;
	}

	if (bwe->inter_arrival != NULL){
		destroy_inter_arrival(bwe->inter_arrival);
		bwe->inter_arrival = NULL;
	}

	if (bwe->trendline_estimator != NULL){
		trendline_destroy(bwe->trendline_estimator);
		bwe->trendline_estimator = NULL;
	}

	free(bwe);
}

static inline void delay_bwe_reset(delay_base_bwe_t* bwe)
{
	if (bwe->inter_arrival != NULL)
		destroy_inter_arrival(bwe->inter_arrival);

	bwe->inter_arrival = create_inter_arrival(0, k_group_time);

	if (bwe->trendline_estimator != NULL)
		trendline_destroy(bwe->trendline_estimator);

	bwe->trendline_estimator = trendline_create(bwe->trendline_window_size, bwe->trendline_smoothing_coeff, bwe->trendline_threshold_gain);
}

static void delay_bwe_process(delay_base_bwe_t* bwe, packet_feedback_t* packet, int64_t now_ts)
{
	uint32_t timestamp;
	uint32_t ts_delta = 0;
	int64_t t_delta = 0;
	int size_delta = 0;

	if (bwe->last_seen_ms == -1 || now_ts > bwe->last_seen_ms + k_timestamp_ms){
		delay_bwe_reset(bwe);
	}

	bwe->last_seen_ms = now_ts;

	timestamp = (uint32_t)(packet->send_ts - bwe->first_ts);
	if (inter_arrival_compute_deltas(bwe->inter_arrival, timestamp, packet->arrival_ts, now_ts, packet->payload_size,
		&ts_delta, &t_delta, &size_delta) == 0){
		/*进行斜率计算*/
		trendline_update(bwe->trendline_estimator, t_delta, ts_delta, packet->arrival_ts);

		/*进行过载检查*/
		overuse_detect(bwe->detector, trendline_slope(bwe->trendline_estimator), ts_delta, bwe->trendline_estimator->num_of_deltas, packet->arrival_ts);
	}
}

static inline bwe_result_t delay_bwe_long_feedback_delay(delay_base_bwe_t* bwe, int64_t arrival_ts)
{
	bwe_result_t result;
	init_bwe_result_null(result);

	aimd_set_estimate(bwe->rate_control, bwe->rate_control->curr_rate * 1 / 2, arrival_ts);
	result.updated = 0;
	result.probe = -1;
	result.bitrate = bwe->rate_control->curr_rate;

	return result;
}

static int delay_bwe_upate(delay_base_bwe_t* bwe, int64_t now_ts, uint32_t acked_bitrate, int overusing, uint32_t* target_bitrate)
{
	uint32_t prev_bitrate;
	rate_control_input_t input;

	input.state = overusing == 0 ? kBwOverusing : bwe->detector->state;
	input.noise_var = 0;
	input.incoming_bitrate = acked_bitrate;

	prev_bitrate = bwe->rate_control->curr_rate;

	*target_bitrate = aimd_update(bwe->rate_control, &input, now_ts);

	return (bwe->rate_control->inited == 0 && prev_bitrate != *target_bitrate) ? 0 : -1;
}

static bwe_result_t delay_bwe_maybe_update(delay_base_bwe_t* bwe, int overusing, uint32_t acked_bitrate, int recovered_from_overuse, int64_t now_ts)
{
	bwe_result_t result;
	init_bwe_result_null(result);

	if (overusing == 0){ /*过载保护，进行带宽下降*/
		if (acked_bitrate > 0 && aimd_time_reduce_further(bwe->rate_control, now_ts, acked_bitrate) == 0){ /*带宽过载，进行aimd方式减小*/
			result.updated = delay_bwe_upate(bwe, now_ts, acked_bitrate, overusing, &result.bitrate);
		}
		else if (acked_bitrate == 0 && bwe->rate_control->inited == 0 
			&& aimd_time_reduce_further(bwe->rate_control, now_ts, bwe->rate_control->curr_rate * 3 / 4 - 1) == 0){ /*带宽过载且没统计到新的acked带宽，进行减半处理*/
			aimd_set_estimate(bwe->rate_control, bwe->rate_control->curr_rate * 3 / 4, now_ts);
			result.updated = 0;
			result.probe = -1;
			result.bitrate = bwe->rate_control->curr_rate;
		}
	}
	else{ /*未过载，进行aimd方式判断是否要加大码率*/
		result.updated = delay_bwe_upate(bwe, now_ts, acked_bitrate, overusing, &result.bitrate);
		result.recovered_from_overuse = recovered_from_overuse;
	}

	return result;
}

bwe_result_t delay_bwe_incoming(delay_base_bwe_t* bwe, packet_feedback_t packets[], int packets_num, uint32_t acked_bitrate, int64_t now_ts)
{
	int i, overusing, delayed_feedback, recovered_from_overuse, prev_state;

	bwe_result_t result;
	init_bwe_result_null(result);

	if (packets_num <= 0)
		return result;

	overusing = -1;
	delayed_feedback = 0;
	recovered_from_overuse = -1;
	prev_state = bwe->detector->state;

	for (i = 0; i < packets_num; ++i){
		if (packets[i].send_ts < bwe->first_ts)
			continue;

		delayed_feedback = -1;

		/*通过发包和收包间隔差计算延迟斜率，通过斜率判断是否过载*/
		delay_bwe_process(bwe, &packets[i], now_ts);
		if (prev_state == kBwUnderusing && bwe->detector->state == kBwNormal)
			recovered_from_overuse = 0;

		prev_state = bwe->detector->state;
	}

	if (bwe->detector->state == kBwOverusing){
		overusing = 0;
		razor_debug("bwe state = kBwOverusing\n");
	}

	if (delayed_feedback == 0){ /*太多次网络feedback事件出现重复，强制的带宽减半并返回*/
		bwe->consecutive_delayed_feedbacks++;
		if (bwe->consecutive_delayed_feedbacks > k_max_failed_count)
			return delay_bwe_long_feedback_delay(bwe, packets[packets_num - 1].arrival_ts);
	}
	else{ /*进行aimd方式码率计算*/
		bwe->consecutive_delayed_feedbacks = 0;
		return delay_bwe_maybe_update(bwe, overusing, acked_bitrate, recovered_from_overuse, now_ts);
	}

	return result;
}

void delay_bwe_rtt_update(delay_base_bwe_t* bwe, uint32_t rtt)
{
	aimd_set_rtt(bwe->rate_control, rtt);
}

int delay_bwe_last_estimate(delay_base_bwe_t* bwe, uint32_t* bitrate)
{
	if (bwe->rate_control->inited == -1)
		return -1;

	*bitrate = bwe->rate_control->curr_rate;
	return 0;
}

void delay_bwe_set_start_bitrate(delay_base_bwe_t* bwe, uint32_t bitrate)
{
	aimd_set_start_bitrate(bwe->rate_control, bitrate);
}

void delay_bwe_set_min_bitrate(delay_base_bwe_t* bwe, uint32_t min_bitrate)
{
	aimd_set_min_bitrate(bwe->rate_control, min_bitrate);
}

void delay_bwe_set_max_bitrate(delay_base_bwe_t* bwe, uint32_t max_bitrate)
{
	aimd_set_max_bitrate(bwe->rate_control, max_bitrate);
}

int64_t delay_bwe_expected_period(delay_base_bwe_t* bwe)
{
	return aimd_get_expected_bandwidth_period(bwe->rate_control);
}



