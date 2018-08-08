/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_feedback_adpater_h_
#define __bbr_feedback_adpater_h_

#include "cf_platform.h"
#include "bbr_common.h"
#include "sender_history.h"
#include "rate_stat.h"

typedef struct
{
	int64_t					feedback_time;

	size_t					data_in_flight;
	size_t					prior_in_flight;

	int						packets_num;
	bbr_packet_info_t		packets[MAX_BBR_FEELBACK_COUNT];
}bbr_feedback_t;

int							bbr_feedback_get_loss(bbr_feedback_t* feeback, bbr_packet_info_t* loss_arr, int arr_size);
int							bbr_feedback_get_received(bbr_feedback_t* feeback, bbr_packet_info_t* recvd_arr, int arr_size);

typedef struct
{
	sender_history_t*		hist;
	bbr_feedback_t			feedback;
	rate_stat_t				acked_bitrate;
}bbr_fb_adapter_t;

void						bbr_feedback_adapter_init(bbr_fb_adapter_t* adapter);
void						bbr_feedback_adapter_destroy(bbr_fb_adapter_t* adapter);

/*添加一个网络发送报文的记录*/
void						bbr_feedback_add_packet(bbr_fb_adapter_t* adapter, uint16_t seq, size_t size, bbr_packet_info_t* info);
void						bbr_feedback_on_feedback(bbr_fb_adapter_t* adapter, bbr_feedback_msg_t* msg);

size_t						bbr_feedback_get_in_flight(bbr_fb_adapter_t* adapter);
int32_t						bbr_feedback_get_birate(bbr_fb_adapter_t* adapter);
#endif


