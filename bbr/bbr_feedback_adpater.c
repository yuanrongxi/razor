#include "bbr_feedback_adpater.h"

#define k_history_cache_ms		60000

static void bbr_free_packet_info(skiplist_item_t key, skiplist_item_t val, void* args)
{
	bbr_packet_info_t* info;
	info = val.ptr;
	if (info != NULL)
		free(info);
}

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

	adapter->cache = skiplist_create(id64_compare, bbr_free_packet_info, NULL);

	init_unwrapper16(&adapter->wrapper);
}

void bbr_feedback_adapter_destroy(bbr_fb_adapter_t* adapter)
{
	if (adapter->hist != NULL){
		sender_history_destroy(adapter->hist);
		adapter->hist = NULL;
	}

	if (adapter->cache != NULL){
		skiplist_destroy(adapter->cache);
		adapter->cache = NULL;
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
	int64_t now_ts, seq;
	packet_feedback_t p;
	bbr_packet_info_t* info;

	skiplist_item_t key, val;
	skiplist_iter_t* iter;

	now_ts = GET_SYS_MS();

	adapter->feedback.packets_num = 0;
	adapter->feedback.prior_in_flight = sender_history_outstanding_bytes(adapter->hist);

	if (sender_history_get(adapter->hist, msg->sample, &p, 0) == 0){
		key.i64 = wrap_uint16(&adapter->wrapper, msg->sample);
		info = calloc(1, sizeof(bbr_packet_info_t));
		info->recv_time = now_ts;
		info->send_time = p.send_ts;
		info->size = p.payload_size;
		info->seq = key.i64;
		val.ptr = info;
		skiplist_insert(adapter->cache, key, val);
	}

	if (skiplist_size(adapter->cache) > 2){
		adapter->feedback.feedback_time = now_ts;

		seq = skiplist_first(adapter->cache)->key.i64;
		SKIPLIST_FOREACH(adapter->cache, iter){
			for (; seq != iter->key.i64; seq++){
				/*¶ªÊ§µÄ±¨ÎÄ*/
				if (sender_history_get(adapter->hist, seq & 0xffff, &p, 0) == 0){
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

			info = iter->val.ptr;
			adapter->feedback.packets[adapter->feedback.packets_num] = *info;
			adapter->feedback.packets_num++;

			if (adapter->feedback.packets_num >= MAX_BBR_FEELBACK_COUNT - 1)
				adapter->feedback.packets_num = 0;

			seq++;
		}
		adapter->feedback.data_in_flight = sender_history_outstanding_bytes(adapter->hist);

		skiplist_clear(adapter->cache);
	}
}

size_t bbr_feedback_get_in_flight(bbr_fb_adapter_t* adapter)
{
	return sender_history_outstanding_bytes(adapter->hist);
}


