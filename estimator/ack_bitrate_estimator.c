/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <stdio.h>
#include "ack_bitrate_estimator.h"
#include "cf_platform.h"

#define k_initial_rate_wnd_ms 500
#define k_rate_wnd_ms 150

ack_bitrate_estimator_t* ack_estimator_create()
{
	ack_bitrate_estimator_t* est = malloc(sizeof(ack_bitrate_estimator_t));

	ack_estimator_reset(est);

	return est;
}

void ack_estimator_destroy(ack_bitrate_estimator_t* est)
{
	if (est != NULL)
		free(est);
}

void ack_estimator_reset(ack_bitrate_estimator_t* est)
{
	est->alr_ended_ts = -1;

	est->curr_win_ms = 0;
	est->prev_ts = -1;
	est->sum = 0;
	est->bitrate_estimate = -1.0f;
	est->bitrate_estimate_var = 50.0f;
}

uint32_t ack_estimator_bitrate_bps(ack_bitrate_estimator_t* est)
{
	if (est->bitrate_estimate < 0)
		return 0;

	return (uint32_t)(est->bitrate_estimate * 1000);
}

void ack_estimator_set_alrended(ack_bitrate_estimator_t* est, int64_t ts)
{
	est->alr_ended_ts = ts;
}

static inline void ack_estimator_mybe_expect_fast_change(ack_bitrate_estimator_t* est, int64_t packet_send_ts)
{
	/*将码率设置到一个变化比较大的范围因子，这个和pacer有关*/
	if (est->alr_ended_ts >= 0 && packet_send_ts > est->alr_ended_ts){
		est->bitrate_estimate_var += 200;
		est->alr_ended_ts = -1;
	}
}

static float ack_estimator_update_window(ack_bitrate_estimator_t* est, int64_t now_ts, size_t size, int rate_wnd_ms)
{
	float bitrate_sample;
	if (now_ts < est->prev_ts){
		est->prev_ts = -1;
		est->sum = 0;
		est->curr_win_ms = 0;
	}

	if (est->prev_ts >= 0){
		est->curr_win_ms += now_ts - est->prev_ts;
		/*跳跃时间超过了一个窗口周期，将统计数据情况重新计算*/
		if (now_ts - est->prev_ts > rate_wnd_ms){
			est->sum = 0;
			est->curr_win_ms %= rate_wnd_ms;
		}
	}

	est->prev_ts = now_ts;
	bitrate_sample = -1.0f;
	if (est->curr_win_ms >= rate_wnd_ms){ /*刚好一个窗口周期，进行码率计算*/
		bitrate_sample = 8.0f * est->sum / rate_wnd_ms;
		est->curr_win_ms -= rate_wnd_ms;
		est->sum = 0;
	}

	est->sum += size;

	return bitrate_sample;
}

static void ack_estimator_update(ack_bitrate_estimator_t* est, int64_t arrival_ts, size_t payload_size)
{
	float bitrate_sample, sample_uncertainty, sample_var, pred_bitrate_estimate_var;
	int rate_windows_ms = k_rate_wnd_ms;

	if (est->bitrate_estimate < 0)
		rate_windows_ms = k_initial_rate_wnd_ms;

	bitrate_sample = ack_estimator_update_window(est, arrival_ts, payload_size, rate_windows_ms);
	if (bitrate_sample < 0.0f)
		return;
	
	if (est->bitrate_estimate < 0.0f){
		est->bitrate_estimate = bitrate_sample;
		return;
	}

	/*引入逼近计算*/
	sample_uncertainty = 10.0f * SU_ABS(est->bitrate_estimate, bitrate_sample) / est->bitrate_estimate;
	sample_var = sample_uncertainty * sample_uncertainty;

	pred_bitrate_estimate_var = est->bitrate_estimate_var + 5.f;

	est->bitrate_estimate = (sample_var * est->bitrate_estimate + pred_bitrate_estimate_var * bitrate_sample) / (sample_var + pred_bitrate_estimate_var);
	est->bitrate_estimate_var = sample_var * pred_bitrate_estimate_var / (sample_var + pred_bitrate_estimate_var);
}

void ack_estimator_incoming(ack_bitrate_estimator_t* est, packet_feedback_t packets[], int size)
{
	int i;

	/*根据接收方的estimator proxy反馈过来的feedback来迭代码率*/
	for (i = 0; i < size; i++){
		if (packets[i].send_ts >= 0){
			ack_estimator_mybe_expect_fast_change(est, packets[i].send_ts);
			ack_estimator_update(est, packets[i].arrival_ts, packets[i].payload_size);
		}
	}
}





