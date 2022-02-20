/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_receiver.h"
#include "bbr_common.h"

#define k_bbr_recv_wnd_size 4096
#define k_bbr_feedback_time 10


bbr_receiver_t* bbr_receive_create(void* handler, send_feedback_func cb)
{
	bbr_receiver_t* cc = calloc(1, sizeof(bbr_receiver_t));
	cc->handler = handler;
	cc->send_cb = cb;

	cc->base_seq = -1;
	cc->max_seq = -1;

	cc->feedback_ts = -1;

	init_unwrapper16(&cc->unwrapper);
	loss_statistics_init(&cc->loss_stat);

	bin_stream_init(&cc->strm);

	cc->cache = skiplist_create(id64_compare, NULL, NULL);

	return cc;
}

void bbr_receive_destroy(bbr_receiver_t* cc)
{
	if (cc == NULL)
		return;

	bin_stream_destroy(&cc->strm);
	loss_statistics_destroy(&cc->loss_stat);

	if (cc->cache){
		skiplist_destroy(cc->cache);
		cc->cache = NULL;
	}

	free(cc);
}

#define BBR_FEEDBACK_WINDOW 10
#define BBR_FEEDBACK_THROLD 16
#define BBR_FEEDBACK_ID_SPACE 32767
void bbr_receive_on_received(bbr_receiver_t* cc, uint16_t seq, uint32_t timestamp, size_t size, int remb)
{
	bbr_feedback_msg_t msg;
	int64_t sequence, start_seq, now_ts;
	skiplist_iter_t* iter;
	skiplist_item_t key, val;

	now_ts = GET_SYS_MS();

	loss_statistics_incoming(&cc->loss_stat, seq, now_ts);

	sequence = wrap_uint16(&cc->unwrapper, seq);
	key.i64 = sequence;
	
	if (cc->max_seq < sequence) {
		cc->max_seq = sequence;

		if (sequence > cc->max_seq + BBR_FEEDBACK_ID_SPACE || cc->base_seq == -1) { /*reset feedback window*/
			cc->base_seq = sequence;
			skiplist_clear(cc->cache);
		}
	}
	else if (skiplist_search(cc->cache, key) != NULL){
		return;
	}

	val.i64 = now_ts;
	skiplist_insert(cc->cache, key, val);

	if (skiplist_size(cc->cache) >= BBR_FEEDBACK_THROLD || (skiplist_size(cc->cache) > 2 && now_ts > cc->feedback_ts + BBR_FEEDBACK_WINDOW)){
		cc->feedback_ts = now_ts;

		msg.flag = bbr_acked_msg;
		msg.sampler_num = 0;
		start_seq = skiplist_first(cc->cache)->key.i64;

		SKIPLIST_FOREACH(cc->cache, iter){
			
			if (start_seq + MAX_BBR_FEELBACK_COUNT - 1 > iter->key.i64){
				msg.samplers[msg.sampler_num].seq = iter->key.i64 & 0xffff;
				msg.samplers[msg.sampler_num].delta_ts = (uint16_t)(now_ts > iter->val.i64 ? (now_ts - iter->val.i64) : 0);
				msg.sampler_num++;
			}
			else{
				if (loss_statistics_calculate(&cc->loss_stat, now_ts, &msg.fraction_loss, &msg.packet_num) == 0)
					msg.flag |= bbr_loss_info_msg;

				bbr_feedback_msg_encode(&cc->strm, &msg);
				cc->send_cb(cc->handler, cc->strm.data, cc->strm.used);
				msg.sampler_num = 0;
				msg.flag = bbr_acked_msg;
				start_seq = iter->key.i64;

				msg.samplers[msg.sampler_num].seq = iter->key.i64 & 0xffff;
				msg.samplers[msg.sampler_num].delta_ts = (uint16_t)(now_ts > iter->val.i64 ? (now_ts - iter->val.i64) : 0);
				msg.sampler_num++;
			}
		}

		if (msg.sampler_num > 0){
			if (loss_statistics_calculate(&cc->loss_stat, now_ts, &msg.fraction_loss, &msg.packet_num) == 0)
				msg.flag |= bbr_loss_info_msg;

			bbr_feedback_msg_encode(&cc->strm, &msg);
			cc->send_cb(cc->handler, cc->strm.data, cc->strm.used);
		}

		skiplist_clear(cc->cache);
	}
}

void bbr_receive_check_acked(bbr_receiver_t* cc)
{

}

void bbr_receive_update_rtt(bbr_receiver_t* cc, int32_t rtt)
{

}

void bbr_receive_set_min_bitrate(bbr_receiver_t* cc, int min_bitrate)
{

}

void bbr_receive_set_max_bitrate(bbr_receiver_t* cc, int max_bitrate)
{

}

