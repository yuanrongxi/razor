/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_loss_rate_filter.h"

#define kLimitNumPackets	50
#define kUpdateIntervalMs	2000

void bbr_loss_filter_init(bbr_loss_rate_filter_t* filter)
{
	filter->total_count = 0;
	filter->lost_count = 0;
	filter->next_loss_update_ms = 0;
	filter->loss_rate_estimate = 0;
}

void bbr_loss_filter_update(bbr_loss_rate_filter_t* filter, int64_t feedback_ts, int packets_sent, int packets_lost)
{
	filter->lost_count += packets_lost;
	filter->total_count += packets_sent;

	/*对丢包比例进行计算*/
	if (filter->next_loss_update_ms + kUpdateIntervalMs < feedback_ts && filter->total_count > kLimitNumPackets){
		filter->loss_rate_estimate = 1.0f * filter->lost_count / filter->total_count;
		filter->lost_count = 0;
		filter->total_count = 0;

		filter->next_loss_update_ms = feedback_ts;
	}
}

double bbr_loss_filter_get(bbr_loss_rate_filter_t* filter)
{
	return filter->loss_rate_estimate;
}
