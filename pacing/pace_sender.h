/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __pace_sender_h_
#define __pace_sender_h_

#include "alr_detector.h"
#include "pacer_queue.h"
#include "razor_api.h"

typedef struct
{
	uint32_t			min_sender_bitrate_kpbs;
	uint32_t			estimated_bitrate;
	uint32_t			pacing_bitrate_kpbs;

	int64_t				last_update_ts;
	int64_t				first_sent_ts;

	alr_detector_t*		alr;
	pacer_queue_t		que;

	interval_budget_t	media_budget;

	/*发包回调函数*/
	void*				handler;
	pace_send_func		send_cb;
}pace_sender_t;

pace_sender_t*			pace_create(void* handler, pace_send_func send_cb, uint32_t que_ms);
void					pace_destroy(pace_sender_t* pace);

void					pace_set_estimate_bitrate(pace_sender_t* pace, uint32_t bitrate_pbs);
void					pace_set_bitrate_limits(pace_sender_t* pace, uint32_t min_sent_bitrate_pbs);

int						pace_insert_packet(pace_sender_t* pace, uint32_t seq, int retrans, size_t size, int64_t now_ts);

int64_t					pace_queue_ms(pace_sender_t* pace);
size_t					pace_queue_size(pace_sender_t* pace);

/*预计发送掉queue中数据的时间*/
int64_t					pace_expected_queue_ms(pace_sender_t* pace);

int64_t					pace_get_limited_start_time(pace_sender_t* pace);

/*尝试发送*/
void					pace_try_transmit(pace_sender_t* pace, int64_t now_ts);

#endif


