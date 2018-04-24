/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "trendline.h"

/*计算曲线斜率来确定过载问题*/
static double linear_fit_slope(delay_hist_t* que, int que_size)
{
	int i;
	double sum_x, sum_y, avg_x, avg_y, numerator, denominator;

	sum_x = sum_y = avg_x = avg_y = 0;
	numerator = 0;
	denominator = 0;

	for (i = 0; i < que_size; ++i){
		sum_x += que[i].arrival_delta;
		sum_y += que[i].smoothed_delay;
	}

	avg_x = sum_x / que_size;
	avg_y = sum_y / que_size;

	for (i = 0; i < que_size; ++i){
		numerator += (que[i].arrival_delta - avg_x) * (que[i].smoothed_delay - avg_y);
		denominator += (que[i].arrival_delta - avg_x) * (que[i].arrival_delta - avg_x);
	}

	if (denominator != 0)
		return numerator / denominator;
	else 
		return 0;
}

#define TRENDLINE_MAX_COUNT 1000

trendline_estimator_t* trendline_create(size_t wnd_size, double smoothing_coef, double threshold_gain)
{
	trendline_estimator_t* est = calloc(1, sizeof(trendline_estimator_t));
	est->window_size = wnd_size;
	est->smoothing_coef = smoothing_coef;
	est->threshold_gain = threshold_gain;

	est->first_arrival_ts = -1;

	est->que = malloc(sizeof(delay_hist_t) * wnd_size);

	return est;
}

void trendline_destroy(trendline_estimator_t* est)
{
	if (est != NULL){
		free(est->que);
		free(est);
	}
}

void trendline_update(trendline_estimator_t* est, double recv_delta_ms, double send_delta_ms, int64_t arrival_ts)
{
	double delta_ms = recv_delta_ms - send_delta_ms;
	delay_hist_t* hist; 

	est->num_of_deltas++;
	if (est->num_of_deltas > TRENDLINE_MAX_COUNT)
		est->num_of_deltas = est->num_of_deltas;

	if (est->first_arrival_ts == -1)
		est->first_arrival_ts = arrival_ts;

	est->acc_delay += delta_ms;
	est->smoothed_delay = est->smoothing_coef * est->smoothed_delay + (1 - est->smoothing_coef) * est->acc_delay;

	hist = &est->que[est->index++ % est->window_size];
	hist->arrival_delta = (double)(arrival_ts - est->first_arrival_ts);
	hist->smoothed_delay = est->smoothed_delay;

	if (est->index >= est->window_size){
		est->trendline = linear_fit_slope(est->que, est->window_size);
		/*printf("trendline = %f\n", est->trendline);*/
	}
}

double trendline_slope(trendline_estimator_t* est)
{
	return est->threshold_gain * est->trendline;
}


