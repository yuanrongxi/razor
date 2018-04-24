/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __delay_base_bwe_h_
#define __delay_base_bwe_h_

#include "cf_platform.h"
#include "inter_arrival.h"
#include "trendline.h"
#include "aimd_rate_control.h"
#include "estimator_common.h"
#include "overuse_detector.h"

#define k_stream_timeout_ms 2000

typedef struct
{
	int updated;
	int probe;
	uint32_t bitrate;
	int recovered_from_overuse;
}bwe_result_t;

#define init_bwe_result_null(result) do{		\
(result).updated = -1;							\
(result).probe = -1;								\
(result).bitrate = 0;							\
(result).recovered_from_overuse = -1;			\
}while (0)


/*发送端基于延迟的码率估计模型实现，通过接收端反馈过来的arrival time和发送的send time进行trendline滤波，评估当前的码率*/
typedef struct
{
	inter_arrival_t*		inter_arrival;
	aimd_rate_controller_t* rate_control;
	trendline_estimator_t*	trendline_estimator;
	overuse_detector_t*		detector;

	int64_t					last_seen_ms;
	int64_t					first_ts;				/*起始时间戳，用于计算相对sender time时间错*/
	size_t					trendline_window_size;
	double					trendline_smoothing_coeff;
	double					trendline_threshold_gain;

	int						consecutive_delayed_feedbacks;

}delay_base_bwe_t;



delay_base_bwe_t*			delay_bwe_create();
void						delay_bwe_destroy(delay_base_bwe_t* bwe);

bwe_result_t				delay_bwe_incoming(delay_base_bwe_t* bwe, packet_feedback_t packets[], int packets_num, uint32_t acked_bitrate, int64_t now_ts);

void						delay_bwe_rtt_update(delay_base_bwe_t* bwe, uint32_t rtt);
int							delay_bwe_last_estimate(delay_base_bwe_t* bwe, uint32_t* bitrate);
void						delay_bwe_set_start_bitrate(delay_base_bwe_t* bwe, uint32_t bitrate);
void						delay_bwe_set_min_bitrate(delay_base_bwe_t* bwe, uint32_t min_bitrate);
void						delay_bwe_set_max_bitrate(delay_base_bwe_t* bwe, uint32_t max_bitrate);

int64_t						delay_bwe_expected_period(delay_base_bwe_t* bwe);

#endif



