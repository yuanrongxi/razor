/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <stdio.h>
#include "inter_arrival.h"
#include "cf_platform.h"

#define BURST_THRESHOLD_MS	5

static inline void reset_group_ts(inter_arrival_t* arr)
{
	arr->cur_ts_group.size = 0;
	arr->cur_ts_group.first_timestamp = 0;
	arr->cur_ts_group.timestamp = 0;
	arr->cur_ts_group.complete_ts = -1;
	arr->cur_ts_group.last_sys_ts = 0;

	arr->prev_ts_group.size = 0;
	arr->prev_ts_group.first_timestamp = 0;
	arr->prev_ts_group.timestamp = 0;
	arr->prev_ts_group.complete_ts = -1;
	arr->prev_ts_group.last_sys_ts = 0;
}

/*判断包是否是乱序，如果报文是前一个group的序列，不进行处理*/
static inline int inter_arrival_in_order(inter_arrival_t* arr, uint32_t ts)
{
	if (arr->cur_ts_group.complete_ts == -1)
		return 0;

	if (arr->cur_ts_group.first_timestamp <= ts)
		return 0;

	return -1;
}

/*判断报文突发发送*/
static inline int belongs_to_burst(inter_arrival_t* arr, uint32_t ts, int64_t arrival_ts)
{
	int64_t arrival_ts_delta;
	uint32_t ts_delta;
	int pro_delta;

	if (arr->burst == -1)
		return -1;

	arrival_ts_delta = arrival_ts - arr->cur_ts_group.complete_ts;
	ts_delta = ts - arr->cur_ts_group.timestamp;
	if (ts_delta == 0)
		return 0;

	pro_delta = (int)(arrival_ts_delta - ts_delta);
	if (pro_delta < 0 && arrival_ts_delta <= BURST_THRESHOLD_MS)
		return 0;

	return -1;
}

static int inter_arrival_new_group(inter_arrival_t* arr, uint32_t ts, int64_t arrival_ts)
{
	uint32_t diff;
	if (arr->cur_ts_group.complete_ts == -1)
		return -1;
	else if (belongs_to_burst(arr, ts, arrival_ts) == 0)
		return -1;
	else{
		diff = ts - arr->cur_ts_group.first_timestamp;
		return (diff > arr->time_group_len_ticks) ? 0 : -1;
	}
}

inter_arrival_t* create_inter_arrival(int burst, uint32_t group_ticks)
{
	inter_arrival_t* arr = calloc(1, sizeof(inter_arrival_t));
	arr->burst = burst;
	arr->time_group_len_ticks = group_ticks;

	reset_group_ts(arr);

	return arr;
}

void destroy_inter_arrival(inter_arrival_t* arr)
{
	if (arr != NULL)
		free(arr);
}

#define OFFSET_THRESHOLD_MS 3000

int inter_arrival_compute_deltas(inter_arrival_t* arr, uint32_t timestamp, int64_t arrival_ts, int64_t system_ts, size_t size, 
	uint32_t* timestamp_delta, int64_t* arrival_delta, int* size_delta)
{
	int ret = -1;
	int64_t arr_delta, sys_delta;
	uint32_t ts_delta;

	if (arr->cur_ts_group.complete_ts == -1){
		arr->cur_ts_group.timestamp = timestamp;
		arr->cur_ts_group.first_timestamp = timestamp;
	}
	else if (inter_arrival_in_order(arr, timestamp) == -1)
		return ret;
	else if (inter_arrival_new_group(arr, timestamp, arrival_ts) == 0){
		if (arr->prev_ts_group.complete_ts >= 0){
			ts_delta = arr->cur_ts_group.timestamp - arr->prev_ts_group.timestamp;
			arr_delta = arr->cur_ts_group.complete_ts - arr->prev_ts_group.complete_ts;
			sys_delta = arr->cur_ts_group.last_sys_ts - arr->prev_ts_group.last_sys_ts;

			if (arr_delta > sys_delta + OFFSET_THRESHOLD_MS){
				reset_group_ts(arr);
				return ret;
			}

			if (arr_delta < 0){
				arr->num_consecutive++;
				if (arr->num_consecutive > 3)
					reset_group_ts(arr);

				return ret;
			}
			else
				arr->num_consecutive = 0;

			*size_delta = arr->cur_ts_group.size - arr->prev_ts_group.size;
			*timestamp_delta = ts_delta;
			*arrival_delta = arr_delta;

			ret = 0;
		}
		arr->prev_ts_group = arr->cur_ts_group;

		arr->cur_ts_group.first_timestamp = timestamp;
		arr->cur_ts_group.timestamp = timestamp;
		arr->cur_ts_group.size = 0;
	}
	else{
		arr->cur_ts_group.timestamp = SU_MAX(arr->cur_ts_group.timestamp, timestamp);
	}

	arr->cur_ts_group.size += size;
	arr->cur_ts_group.complete_ts = arrival_ts;
	arr->cur_ts_group.last_sys_ts = system_ts;

	return ret;
}
