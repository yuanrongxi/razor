/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __aimd_rate_control_h_
#define __aimd_rate_control_h_

#include <stdint.h>
#include "cf_platform.h"
#include "estimator_common.h"

typedef struct
{
	uint32_t		min_rate;
	uint32_t		max_rate;
	uint32_t		curr_rate;

	float			avg_max_bitrate_kbps;
	float			var_max_bitrate_kbps;

	int				state;
	int				region;

	int64_t			time_last_bitrate_change;
	int64_t			time_first_incoming_estimate;

	uint32_t		rtt;

	int				inited;
	int				in_experiment;

	float			beta;

	int				last_decrease;
}aimd_rate_controller_t;

aimd_rate_controller_t*		aimd_create(uint32_t max_rate, uint32_t min_rate);
void						aimd_destroy(aimd_rate_controller_t* aimd);

void						aimd_set_start_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate);
int64_t						aimd_get_feelback_interval(aimd_rate_controller_t* aimd);

int							aimd_time_reduce_further(aimd_rate_controller_t* aimd, int64_t cur_ts, uint32_t incoming_rate);
void						aimd_set_rtt(aimd_rate_controller_t* aimd, uint32_t rtt);
void						aimd_set_min_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate);
void						aimd_set_max_bitrate(aimd_rate_controller_t* aimd, uint32_t bitrate);
uint32_t					aimd_update(aimd_rate_controller_t* aimd, rate_control_input_t* input, int64_t cur_ts);
void						aimd_set_estimate(aimd_rate_controller_t* aimd, int bitrate, int64_t cur_ts);
int							aimd_get_near_max_inc_rate(aimd_rate_controller_t* aimd);
int							aimd_get_expected_bandwidth_period(aimd_rate_controller_t* aimd);

#endif 



