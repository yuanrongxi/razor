/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "overuse_detector.h"
#include "cf_platform.h"

#include <math.h>
#include <stdlib.h>

static double kMaxAdaptOffsetMs = 15.0;
static double kOverUsingTimeThreshold = 20;
static int kMinNumDeltas = 60;
static int kMaxTimeDeltaMs = 100;

overuse_detector_t* overuse_create()
{
	overuse_detector_t* detector = calloc(1, sizeof(overuse_detector_t));

	detector->k_up = 0.0187;
	detector->k_down = 0.039;
	detector->ouveusing_time_threshold = kOverUsingTimeThreshold;
	detector->threshold = 12.5;
	detector->update_ts = -1;
	detector->time_over_using = -1;
	detector->state = kBwNormal;

	return detector;
}

void overuse_destroy(overuse_detector_t* detector)
{
	if (detector != NULL)
		free(detector);
}

/*更新过载的阈值*/
static void overuse_update_threshold(overuse_detector_t* detector, double modified_offset, int64_t cur_ts)
{
	double k;
	int64_t time_delta;

	if (detector->update_ts == -1)
		detector->update_ts = cur_ts;

	if (fabs(modified_offset) > detector->threshold + kMaxAdaptOffsetMs){
		detector->update_ts = cur_ts;
		return;
	}

	k = fabs(modified_offset) < detector->threshold ? detector->k_down : detector->k_up;
	time_delta = SU_MIN(cur_ts - detector->update_ts, kMaxTimeDeltaMs);

	detector->threshold += k * (fabs(modified_offset) - detector->threshold) * time_delta;
	detector->threshold = SU_MAX(6.0f, SU_MIN(600.0f, detector->threshold));

	detector->update_ts = cur_ts;
}

/*过载检测*/
int overuse_detect(overuse_detector_t* detector, double offset, double ts_delta, int num_of_deltas, int64_t cur_ts)
{
	double T;

	if (num_of_deltas < 2)
		return kBwNormal;

	T = SU_MIN(num_of_deltas, kMinNumDeltas) * offset;
	if (T > detector->threshold){ /*计算累计的overusing值*/
		if (detector->time_over_using == -1)
			detector->time_over_using = ts_delta / 2;
		else
			detector->time_over_using += ts_delta;

		detector->overuse_counter++;

		if (detector->time_over_using > detector->ouveusing_time_threshold && detector->overuse_counter > 1){
			if (offset >= detector->prev_offset){ /*连续两次以上传输延迟增量增大，表示网络已经过载了，需要进行带宽减小*/
				detector->time_over_using = 0;
				detector->overuse_counter = 0;
				detector->state = kBwOverusing;
			}
		}
	}
	else if (T < -detector->threshold){ /*网络延迟增量逐步缩小，需要加大带宽码率*/
		detector->time_over_using = -1;
		detector->overuse_counter = 0;
		detector->state = kBwUnderusing;
	}
	else{
		detector->time_over_using = -1;
		detector->overuse_counter = 0;
		detector->state = kBwNormal;
	}

	detector->prev_offset = offset;
	overuse_update_threshold(detector, T, cur_ts);

	return detector->state;
}








