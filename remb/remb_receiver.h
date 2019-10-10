/*-
* Copyright (c) 2017-2019 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __remb_receiver_h_
#define __remb_receiver_h_

#include "cf_platform.h"
#include "cc_loss_stat.h"
#include "razor_api.h"
#include "rate_stat.h"
#include "cf_stream.h"

typedef struct
{
	razor_receiver_t			receiver;

	int64_t						feedback_ts;
	int							min_bitrate;
	int							max_bitrate;

	/*信息反馈函数*/
	void*						handler;
	send_feedback_func			send_cb;

	cc_loss_statistics_t		loss_stat;
	bin_stream_t				strm;
	rate_stat_t					rate;
}remb_receiver_t;


remb_receiver_t*				remb_receive_create(void* handler, send_feedback_func cb);
void							remb_receive_destroy(remb_receiver_t* r);

void							remb_receive_heartbeat(remb_receiver_t* r);
void							remb_receive_check_acked(remb_receiver_t* r);
void							remb_receive_on_received(remb_receiver_t* r, uint16_t seq, uint32_t timestamp, size_t size, int remb);

void							remb_receive_update_rtt(remb_receiver_t* r, uint32_t rtt);
void							remb_receive_set_min_bitrate(remb_receiver_t* r, int min_bitrate);
void							remb_receive_set_max_bitrate(remb_receiver_t* r, int max_bitrate);

#endif
