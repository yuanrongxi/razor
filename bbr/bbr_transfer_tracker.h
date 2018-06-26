/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __bbr_transfer_tracker_h_
#define __bbr_transfer_tracker_h_

#include "cf_platform.h"
#include "cf_list.h"

typedef struct
{
	int64_t			ack_time;
	int64_t			send_time;
	size_t			ack_data_size;
}tracker_result_t;

typedef struct
{
	base_list_t*	l;
	size_t			size_sum;
}bbr_trans_tracker_t;

bbr_trans_tracker_t*		tracker_create();
void						tracker_destroy(bbr_trans_tracker_t* tracker);

void						tracker_add_sample(bbr_trans_tracker_t* tracker, size_t delta_size, int64_t send_time, int64_t ack_time);
void						tracker_clear_old(bbr_trans_tracker_t* tracker, int64_t excluding_end);

tracker_result_t			tracker_get_rates_by_ack_time(bbr_trans_tracker_t* tracker, int64_t covered_start, int64_t including_end);

#endif



