/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __trendline_h_
#define __trendline_h_

#include "cf_platform.h"

/*通过延迟的增长趋势确定过载参数*/

typedef struct
{
	double arrival_delta;
	double smoothed_delay;
}delay_hist_t;

typedef struct
{
	size_t		window_size;
	double		smoothing_coef;
	double		threshold_gain;

	uint32_t	num_of_deltas;
	int64_t		first_arrival_ts;

	double		acc_delay;
	double		smoothed_delay;

	double		trendline;

	int			index;
	delay_hist_t* que;		/*最近的曲线趋势历史点数据*/

}trendline_estimator_t;


trendline_estimator_t*	trendline_create(size_t wnd_size, double smoothing_coef, double threshold_gain);
void					trendline_destroy(trendline_estimator_t* est);

void					trendline_update(trendline_estimator_t* est, double recv_delta_ms, double send_delta_ms, int64_t arrival_ts);
double					trendline_slope(trendline_estimator_t* est);

#endif