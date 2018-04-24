/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __overuse_detector_h_
#define __overuse_detector_h_

#include "estimator_common.h"
#include <stdint.h>

/*ÍøÂç¹ıÔØ¼ì²âÆ÷£¬ÒÆÖ²ÓÚwebRTC*/

typedef struct
{
	double k_up;
	double k_down;
	double ouveusing_time_threshold;
	double threshold;
	double time_over_using;
	double prev_offset;
	
	int64_t update_ts;
	int overuse_counter;

	int	state;
}overuse_detector_t;

overuse_detector_t*	overuse_create();
void				overuse_destroy(overuse_detector_t* detector);
int					overuse_detect(overuse_detector_t* detector, double offset, double ts_delta, int num_of_deltas, int64_t cur_ts);


#endif



