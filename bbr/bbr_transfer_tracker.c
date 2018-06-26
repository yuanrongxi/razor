/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "bbr_transfer_tracker.h"

typedef struct
{
	int64_t			ack_time;
	int64_t			send_time;
	size_t			delta_size;
	size_t			size_sum;
}tracker_sample_t;


bbr_trans_tracker_t* tracker_create()
{
	bbr_trans_tracker_t* tracker = calloc(1, sizeof(bbr_trans_tracker_t));
	tracker->l = create_list();

	return tracker;
}

void tracker_destroy(bbr_trans_tracker_t* tracker)
{
	tracker_sample_t* sample;
	base_list_unit_t* iter;

	if (tracker == NULL)
		return;

	LIST_FOREACH(tracker->l, iter){
		sample = (tracker_sample_t*)iter->pdata;
		if (sample != NULL)
			free(sample);
	}

	destroy_list(tracker->l);

	free(tracker);
}

void tracker_add_sample(bbr_trans_tracker_t* tracker, size_t delta_size, int64_t send_time, int64_t ack_time)
{
	tracker_sample_t* sample;

	tracker->size_sum += delta_size;

	if (list_size(tracker->l) > 0){
		sample = list_back(tracker->l);
		if (sample->ack_time == ack_time){
			sample->send_time = send_time;
			sample->size_sum = tracker->size_sum;
		}
		else
			goto new_sample;
	}
	else{
new_sample:
		sample = calloc(1, sizeof(tracker_sample_t));
		sample->ack_time = ack_time;
		sample->send_time = send_time;
		sample->delta_size = delta_size;
		sample->size_sum = tracker->size_sum;

		list_push(tracker->l, sample);
	}
}

void tracker_clear_old(bbr_trans_tracker_t* tracker, int64_t excluding_end)
{
	tracker_sample_t* sample;
	
	while (list_size(tracker->l) > 0){
		sample = list_front(tracker->l);
		if (sample->ack_time < excluding_end){
			free(sample);
			list_pop(tracker->l);
		}
		else
			break;
	}
}

tracker_result_t tracker_get_rates_by_ack_time(bbr_trans_tracker_t* tracker, int64_t covered_start, int64_t including_end)
{
	base_list_unit_t* iter;
	tracker_result_t result = { 0, 0, 0 };
	tracker_sample_t* window_begin, *window_end, *sample;

	window_begin = NULL;
	window_end = NULL;

	if (list_size(tracker->l) <= 0)
		goto err;

	sample = list_front(tracker->l);
	if (sample->ack_time < including_end)
		window_begin = sample;

	sample = list_back(tracker->l);
	if (sample->ack_time > covered_start)
		window_end = sample;

	LIST_FOREACH(tracker->l, iter){
		sample = iter->pdata;
		if (sample->ack_time < covered_start)
			window_begin = sample;
		else if (sample->ack_time >= including_end){
			window_end = sample;
			break;
		}
	}

	if (window_begin != NULL && window_end != NULL){
		result.ack_time = window_end->ack_time - window_begin->ack_time;
		result.send_time = window_end->send_time - window_begin->send_time;
		result.ack_data_size = window_end->size_sum - window_begin->size_sum;
	}
	else{
err:
		result.send_time = including_end - covered_start;
	}

	return result;
}




