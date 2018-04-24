/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __alr_detector_h_
#define __alr_detector_h_

#include "cf_platform.h"
#include "interval_budget.h"

#define k_alr_start_buget_percent		80
#define k_alr_stop_buget_percent		50
#define k_alr_banwidth_useage_percent	60

/*alr detector是一个根据当前评估出来的码率和时间来决定发送数据大小一个窗口机制，这个机制防止单位时间内发送了太多的包造成网络风暴*/
typedef struct
{
	interval_budget_t	budget;
	int64_t				alr_started_ts;
}alr_detector_t;

alr_detector_t*			alr_detector_create();
void					alr_detector_destroy(alr_detector_t* alr);

void					alr_detector_bytes_sent(alr_detector_t* alr, size_t bytes, int64_t delta_ts);
void					alr_detector_set_bitrate(alr_detector_t* alr, int bitrate_bps);

int64_t					alr_get_app_limited_started_ts(alr_detector_t* alr);

#endif



