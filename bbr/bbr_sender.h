/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_sender_h_
#define __bbr_sender_h_

#include "bbr_controller.h"
#include "bbr_feedback_adpater.h"
#include "bbr_pacer.h"
#include "razor_api.h"

typedef struct
{
	razor_sender_t				sender;
	/*上一次评估的网络状态*/
	uint32_t					last_bitrate_bps;
	uint8_t						last_fraction_loss;
	uint32_t					last_rtt;

	bbr_network_ctrl_update_t	info;

	int64_t						notify_ts;

	bbr_controller_t*			bbr;
	bbr_pacer_t*				pacer;
	bbr_fb_adapter_t			feedback;

	bin_stream_t				strm;

	void*						trigger;					/*码率改变后需要通知给通信层的trigger*/
	bitrate_changed_func		trigger_cb;					/*通知函数*/

	double						encoding_rate_ratio;
	uint32_t					max_bitrate;
	uint32_t					min_bitrate;

}bbr_sender_t;

bbr_sender_t*					bbr_sender_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms, int padding);
void							bbr_sender_destroy(bbr_sender_t* s);

void							bbr_sender_heartbeat(bbr_sender_t* s, int64_t now_ts);

int								bbr_sender_add_pace_packet(bbr_sender_t* s, uint32_t packet_id, int retrans, size_t size);
/*这里的seq是transport的自增长ID，即使包重发，这个ID也是不一样的*/
void							bbr_sender_send_packet(bbr_sender_t* s, uint16_t seq, size_t size);

void							bbr_sender_on_feedback(bbr_sender_t* s, uint8_t* feedback, int feedback_size);

void							bbr_sender_update_rtt(bbr_sender_t* s, int32_t rtt);
void							bbr_sender_set_bitrates(bbr_sender_t* s, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

int64_t							bbr_sender_get_pacer_queue_ms(bbr_sender_t* s);
int64_t							bbr_sender_get_first_packet_ts(bbr_sender_t* s);

#endif



