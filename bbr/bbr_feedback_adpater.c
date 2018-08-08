/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_feedback_adpater.h"

#define k_history_cache_ms		60000
#define k_rate_window_size 1000
#define k_rate_scale 8000

int bbr_feedback_get_loss(bbr_feedback_t* feedback, bbr_packet_info_t* loss_arr, int arr_size)
{
	int i, count;

	count = 0;
	for (i = 0; i < feedback->packets_num; ++i){
		if (feedback->packets[i].recv_time <= 0)
			loss_arr[count++] = feedback->packets[i];
	}

	return count;
}

int bbr_feedback_get_received(bbr_feedback_t* feedback, bbr_packet_info_t* recvd_arr, int arr_size)
{
	int i, count;

	count = 0;
	for (i = 0; i < feedback->packets_num; ++i){
		if (feedback->packets[i].recv_time > 0)
			recvd_arr[count++] = feedback->packets[i];
	}
	return count;
}

void bbr_feedback_adapter_init(bbr_fb_adapter_t* adapter)
{
	adapter->hist = sender_history_create(k_history_cache_ms);

	adapter->feedback.data_in_flight = 0;
	adapter->feedback.prior_in_flight = 0;
	adapter->feedback.feedback_time = GET_SYS_MS();
	adapter->feedback.packets_num = 0;

	rate_stat_init(&adapter->acked_bitrate, k_rate_window_size, k_rate_scale);
}

void bbr_feedback_adapter_destroy(bbr_fb_adapter_t* adapter)
{
	if (adapter->hist != NULL){
		sender_history_destroy(adapter->hist);
		adapter->hist = NULL;
	}

	rate_stat_destroy(&adapter->acked_bitrate);
}

void bbr_feedback_add_packet(bbr_fb_adapter_t* adapter, uint16_t seq, size_t size, bbr_packet_info_t* info)
{
	packet_feedback_t packet;
	int64_t now_ts = GET_SYS_MS();

	packet.arrival_ts = -1;
	packet.create_ts = packet.send_ts = now_ts;
	packet.payload_size = size;
	packet.sequence_number = seq;

	info->data_in_flight = sender_history_outstanding_bytes(adapter->hist);

	sender_history_add(adapter->hist, &packet);

	info->send_time = now_ts;
	info->recv_time = -1;
	info->size = size;
	info->seq = wrap_uint16(&adapter->hist->wrapper, seq);
}

void bbr_feedback_on_feedback(bbr_fb_adapter_t* adapter, bbr_feedback_msg_t* msg)
{
	uint16_t seq;
	int i;
	int64_t now_ts;
	packet_feedback_t p;

	now_ts = GET_SYS_MS();

	adapter->feedback.packets_num = 0;
	adapter->feedback.feedback_time = now_ts;
	adapter->feedback.prior_in_flight = sender_history_outstanding_bytes(adapter->hist);
	seq = msg->samplers[0].seq;
	for (i = 0; i < msg->sampler_num; i++){
		for (; seq != msg->samplers[i].seq; seq++){
			/*¶ªÊ§µÄ±¨ÎÄ*/
			if (sender_history_get(adapter->hist, seq, &p, 0) == 0){
				adapter->feedback.packets[adapter->feedback.packets_num].seq = seq;
				adapter->feedback.packets[adapter->feedback.packets_num].recv_time = -1;
				adapter->feedback.packets[adapter->feedback.packets_num].size = p.payload_size;
				adapter->feedback.packets[adapter->feedback.packets_num].send_time = p.send_ts;
				adapter->feedback.packets[adapter->feedback.packets_num].data_in_flight = 0;
				adapter->feedback.packets_num++;
				if (adapter->feedback.packets_num >= MAX_BBR_FEELBACK_COUNT - 1)
					adapter->feedback.packets_num = 0;
			}
		}

		if (sender_history_get(adapter->hist, msg->samplers[i].seq, &p, 0) == 0){
			adapter->feedback.packets[adapter->feedback.packets_num].seq = wrap_uint16(&adapter->hist->wrapper, msg->samplers[i].seq);
			adapter->feedback.packets[adapter->feedback.packets_num].recv_time = now_ts - msg->samplers[i].delta_ts;
			adapter->feedback.packets[adapter->feedback.packets_num].send_time = p.send_ts;
			adapter->feedback.packets[adapter->feedback.packets_num].size = p.payload_size;
			adapter->feedback.packets[adapter->feedback.packets_num].data_in_flight = 0;
			adapter->feedback.packets_num++;
			if (adapter->feedback.packets_num >= MAX_BBR_FEELBACK_COUNT - 1)
				adapter->feedback.packets_num = 0;

			rate_stat_update(&adapter->acked_bitrate, p.payload_size, now_ts);
		}

		seq++;
	}

	adapter->feedback.data_in_flight = sender_history_outstanding_bytes(adapter->hist);
}

size_t bbr_feedback_get_in_flight(bbr_fb_adapter_t* adapter)
{
	return sender_history_outstanding_bytes(adapter->hist);
}

int32_t bbr_feedback_get_birate(bbr_fb_adapter_t* adapter)
{
	return rate_stat_rate(&adapter->acked_bitrate, GET_SYS_MS());
}


