#include "bbr_feedback_adpater.h"

#define k_history_cache_ms		60000

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
}

void bbr_feedback_adapter_destroy(bbr_fb_adapter_t* adapter)
{
	if (adapter->hist != NULL){
		sender_history_destroy(adapter->hist);
		adapter->hist = NULL;
	}
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
	int32_t i;
	int64_t now_ts;
	packet_feedback_t p;

	now_ts = GET_SYS_MS();
	adapter->feedback.feedback_time = now_ts;
	adapter->feedback.packets_num = 0;

	adapter->feedback.prior_in_flight = sender_history_outstanding_bytes(adapter->hist);
	seq = msg->base_seq & 0xffff;
	
	for (i = 0; i < msg->samples_num; i++){
		for (; seq != msg->samples[i]; seq++){
			/*¶ªÊ§µÄ±¨ÎÄ*/
			if (sender_history_get(adapter->hist, seq, &p, 0) == 0){
				adapter->feedback.packets[adapter->feedback.packets_num].seq = wrap_uint16(&adapter->hist->wrapper, seq);
				adapter->feedback.packets[adapter->feedback.packets_num].recv_time = -1;
				adapter->feedback.packets[adapter->feedback.packets_num].size = p.payload_size;
				adapter->feedback.packets[adapter->feedback.packets_num].send_time = p.send_ts;
				adapter->feedback.packets[adapter->feedback.packets_num].data_in_flight = 0;
				adapter->feedback.packets_num++;
				if (adapter->feedback.packets_num >= MAX_BBR_FEELBACK_COUNT - 1)
					adapter->feedback.packets_num = 0;
			}
		}

		if (sender_history_get(adapter->hist, msg->samples[i], &p, 1) == 0){
			adapter->feedback.packets[adapter->feedback.packets_num].seq = wrap_uint16(&adapter->hist->wrapper, seq);
			adapter->feedback.packets[adapter->feedback.packets_num].recv_time = now_ts;
			adapter->feedback.packets[adapter->feedback.packets_num].size = p.payload_size;
			adapter->feedback.packets[adapter->feedback.packets_num].send_time = p.send_ts;
			adapter->feedback.packets[adapter->feedback.packets_num].data_in_flight = 0;
			adapter->feedback.packets_num++;
		}

		seq ++;
	}

	adapter->feedback.data_in_flight = sender_history_outstanding_bytes(adapter->hist);
}

size_t bbr_feedback_get_in_flight(bbr_fb_adapter_t* adapter)
{
	return sender_history_outstanding_bytes(adapter->hist);
}


