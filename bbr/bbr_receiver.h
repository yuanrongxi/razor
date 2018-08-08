/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_receive_controller_h_
#define __bbr_receive_controller_h_

#include "cf_platform.h"
#include "cc_loss_stat.h"
#include "razor_api.h"
#include "cf_stream.h"
#include "cf_skiplist.h"

typedef struct
{
	razor_receiver_t			receiver;

	cc_loss_statistics_t		loss_stat;

	/*信息反馈函数*/
	void*						handler;
	send_feedback_func			send_cb;

	/*收包信息*/
	int64_t						feedback_ts;		/*feedbak时间戳，UNIX绝对时间，毫秒为单位*/

	int64_t						base_seq;			/*窗口反馈的起始ID*/
	skiplist_t*					cache;

	cf_unwrapper_t				unwrapper;			/*ID恢复器*/
	bin_stream_t				strm;
}bbr_receiver_t;

bbr_receiver_t*					bbr_receive_create(void* handler, send_feedback_func cb);
void							bbr_receive_destroy(bbr_receiver_t* cc);

void							bbr_receive_check_acked(bbr_receiver_t* cc);
void							bbr_receive_on_received(bbr_receiver_t* cc, uint16_t seq, uint32_t timestamp, size_t size, int remb);

/*暂时无需实现的接口，因为BBR只需要反馈接收端报文信息就可以进行拥塞控制*/
void							bbr_receive_update_rtt(bbr_receiver_t* cc, int32_t rtt);
void							bbr_receive_set_min_bitrate(bbr_receiver_t* cc, int min_bitrate);
void							bbr_receive_set_max_bitrate(bbr_receiver_t* cc, int max_bitrate);

#endif



