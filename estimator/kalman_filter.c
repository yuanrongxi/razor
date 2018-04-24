/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "kalman_filter.h"
#include "estimator_common.h"

#include <math.h>

kalman_filter_t* kalman_filter_create()
{
	kalman_filter_t* filter = calloc(1, sizeof(kalman_filter_t));

	filter->slope = 8.0 / 512.0;
	filter->var_noise = 0.0;
	filter->var_noise = 50.0;

	filter->E[0][0] = 100;
	filter->E[1][1] = 1e-1;
	filter->E[0][1] = filter->E[1][0] = 0;

	filter->process_noise[0] = 1e-13;
	filter->process_noise[1] = 1e-3;

	return filter;
}

void kalman_filter_destroy(kalman_filter_t* kalman)
{
	if (kalman != NULL)
		free(kalman);
}

/*查找最近60个样本中最小的报文帧发送间隔*/
static double kalman_filter_update_min_period(kalman_filter_t* kalman, double ts_delta)
{
	double min_period = ts_delta;
	uint32_t i, size;

	kalman->history[kalman->index % HISTORY_FRAME_SIZE] = ts_delta;
	++kalman->index;

	size = SU_MIN(kalman->index, HISTORY_FRAME_SIZE);
	for (i = 0; i < size; ++i){
		if (kalman->history[i] < min_period)
			min_period = kalman->history[i];
	}

	return min_period;
}

/*调整kalman filter的噪声参数,移植webRTC中的代码*/
static void kalman_filter_update_noise(kalman_filter_t* kalman, double residual, double ts_delta, int stable_state)
{
	double alpha, beta;

	if (stable_state != 0)
		return;

	alpha = 0.01;
	if (kalman->num_of_deltas > 10 * 30)
		alpha = 0.002;

	beta = pow(1 - alpha, ts_delta * 30.0 / 1000.0);
	kalman->avg_noise = beta * kalman->avg_noise + (1 - beta) * residual;
	kalman->var_noise = beta * kalman->var_noise + (1 - beta) * (kalman->avg_noise - residual) * (kalman->avg_noise - residual);
	if (kalman->var_noise < 1)
		kalman->var_noise = 1;
}

#define DELTA_COUNTER_MAX 1000

void kalman_filter_update(kalman_filter_t* kalman, int64_t arrival_ts_delta, double ts_delta, int size_delta, int state, int64_t now_ms)
{
	double min_frame_period, t_ts_delta, fs_delta;
	double residual, max_residual, denom, e00, e01;
	int positive_semi_definite;

	min_frame_period = kalman_filter_update_min_period(kalman, ts_delta);
	t_ts_delta = arrival_ts_delta - ts_delta;
	fs_delta = size_delta;

	++kalman->num_of_deltas;

	if (kalman->num_of_deltas > DELTA_COUNTER_MAX)
		kalman->num_of_deltas = DELTA_COUNTER_MAX;

	kalman->E[0][0] += kalman->process_noise[0];
	kalman->E[1][1] += kalman->process_noise[1];

	if ((state == kBwOverusing && kalman->offset < kalman->prev_offset) ||
		(state == kBwUnderusing && kalman->offset > kalman->prev_offset)) {
		kalman->E[1][1] += 10 * kalman->process_noise[1];
	}

	double h[2] = { fs_delta, 1.0 };
	double Eh[2] = { kalman->E[0][0] * h[0] + kalman->E[0][1] * h[1], kalman->E[1][0] * h[0] + kalman->E[1][1] * h[1] };

	residual = t_ts_delta - kalman->slope * h[0] - kalman->offset;

	int in_stable_state = (state == kBwNormal) ? 0 : -1;
	max_residual = 3.0 * sqrt(kalman->var_noise);

	/* We try to filter out very late frames. For instance periodic key frames doesn't fit the Gaussian model well*/
	if (fabs(residual) < max_residual)
		kalman_filter_update_noise(kalman, residual, min_frame_period, in_stable_state);
	else 
		kalman_filter_update_noise(kalman, residual < 0 ? -max_residual : max_residual, min_frame_period, in_stable_state);

	denom = kalman->var_noise + h[0] * Eh[0] + h[1] * Eh[1];

	double K[2] = { Eh[0] / denom, Eh[1] / denom };

	double IKh[2][2] = { { 1.0 - K[0] * h[0], -K[0] * h[1] },
					   { -K[1] * h[0], 1.0 - K[1] * h[1] } };

	e00 = kalman->E[0][0];
	e01 = kalman->E[0][1];

	/*Update state*/
	kalman->E[0][0] = e00 * IKh[0][0] + kalman->E[1][0] * IKh[0][1];
	kalman->E[0][1] = e01 * IKh[0][0] + kalman->E[1][1] * IKh[0][1];
	kalman->E[1][0] = e00 * IKh[1][0] + kalman->E[1][0] * IKh[1][1];
	kalman->E[1][1] = e01 * IKh[1][0] + kalman->E[1][1] * IKh[1][1];

	positive_semi_definite = (kalman->E[0][0] + kalman->E[1][1] >= 0 && kalman->E[0][0] * kalman->E[1][1] - kalman->E[0][1] * kalman->E[1][0] >= 0 && kalman->E[0][0] >= 0) ? 0 : -1;
	if (positive_semi_definite != 0)
		printf("The over-use estimator's covariance matrix is no longer semi-definite.\n");

	kalman->slope = kalman->slope + K[0] * residual;
	kalman->prev_offset = kalman->offset;
	kalman->offset = kalman->offset + K[1] * residual;

	/*printf("kc = %f, km = %f, slope: %f, var noise = %f\n", K[0], K[1], kalman->slope, kalman->var_noise);*/
}


