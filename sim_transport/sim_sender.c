/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_internal.h"
#include <assert.h>

/*单个报文发送的最大次数*/
#define MAX_SEND_COUNT		10

/*处理来自razor的码率调节通告,这个函数需要剥离FEC和重传需要的码率*/
static void sim_bitrate_change(void* trigger, uint32_t bitrate, uint8_t fraction_loss, uint32_t rtt)
{
	sim_session_t* s = (sim_session_t*)trigger;
	sim_sender_t* sender = s->sender;

	uint32_t overhead_bitrate, per_packets_second, payload_bitrate, video_bitrate_kbps;
	double loss;
	uint32_t packet_size_bit = (SIM_SEGMENT_HEADER_SIZE + SIM_VIDEO_SIZE) * 8;

	/*计算这个码率下每秒能发送多少个报文*/
	per_packets_second = (bitrate + packet_size_bit - 1) / packet_size_bit;
	/*计算传输协议头需要占用的码率*/
	overhead_bitrate = per_packets_second * SIM_SEGMENT_HEADER_SIZE * 8;
	/*计算视频数据可利用的码率*/
	payload_bitrate = bitrate - overhead_bitrate;

	/*计算丢包率，用平滑遗忘算法进行逼近，webRTC用的是单位时间内最大和时间段平滑*/
	if (fraction_loss < s->loss_fraction)
		s->loss_fraction = (s->loss_fraction * 15 + fraction_loss) / 16;
	else
		s->loss_fraction = (fraction_loss + s->loss_fraction) / 2;

	loss = s->loss_fraction / 255.0;

	/*留出7%码率做nack和feedback*/
	if (loss > 0.5) /*重传的带宽不能大于半*/
		loss = 0.5;

	/*计算视频编码器的码率,单位kbps*/
	video_bitrate_kbps = (uint32_t)((1.0 - loss) * payload_bitrate) / 1000;

	/*sim_info("loss = %f, bitrate = %u, video_bitrate_kbps = %u\n", loss, bitrate, video_bitrate_kbps);*/
	/*通知上层进行码率调整*/
	s->change_bitrate_cb(s->event, video_bitrate_kbps, loss > 0 ? 1 : 0);
}

static void sim_send_packet(void* handler, uint32_t send_id, int fec, size_t size, int padding)
{
	sim_header_t header;
	sim_segment_t* seg;
	sim_fec_t* fec_packet;

	skiplist_iter_t* it;
	skiplist_item_t key;
	int64_t now_ts;

	sim_sender_t* sender = (sim_sender_t*)handler;
	sim_session_t* s = sender->s;
	sim_pad_t pad;

	now_ts = GET_SYS_MS();

	if (padding == 1){
		pad.transport_seq = sender->transport_seq_seed++;
		pad.send_ts = (uint32_t)(now_ts - sender->first_ts);
		pad.data_size = SU_MIN(size, SIM_VIDEO_SIZE);

		/*将发送记录送入拥塞对象中进行bwe对象做延迟估算*/
		sender->cc->on_send(sender->cc, pad.transport_seq, pad.data_size + SIM_SEGMENT_HEADER_SIZE);

		INIT_SIM_HEADER(header, SIM_PAD, s->uid);
		sim_encode_msg(&s->sstrm, &header, &pad);

		sim_session_network_send(s, &s->sstrm);

		if(s->loss_fraction == 0)
			s->loss_fraction = 1;
	}
	else if (fec == 0){
		key.u32 = send_id;
		it = skiplist_search(sender->segs_cache, key);
		if (it == NULL){
			sim_debug("send packet to network failed, send_id = %u\n", send_id);
			return;
		}

		seg = it->val.ptr;
		/*每发送一次，就进行传输序号+1*/
		seg->transport_seq = sender->transport_seq_seed++;
		/*send_ts是相对当前帧产生的时间之差，用于接收端计算发送时间间隔*/
		seg->send_ts = (uint16_t)(now_ts - sender->first_ts - seg->timestamp);
		/*将发送记录送入拥塞对象中进行bwe对象做延迟估算*/
		sender->cc->on_send(sender->cc, seg->transport_seq, seg->data_size + SIM_SEGMENT_HEADER_SIZE);

		INIT_SIM_HEADER(header, SIM_SEG, s->uid);
		sim_encode_msg(&s->sstrm, &header, seg);

		sim_session_network_send(s, &s->sstrm);

		/*sim_debug("send packet id = %u, transport_seq = %u\n", packet_id, sender->transport_seq_seed - 1);*/
	}
	else if (fec == 1){
		key.u32 = send_id;
		it = skiplist_search(sender->fecs_cache, key);
		if (it == NULL){
			sim_debug("send fec to network failed, send_id = %u\n", send_id);
			return;
		}

		fec_packet = it->val.ptr;
		fec_packet->transport_seq = sender->transport_seq_seed++;
		fec_packet->send_ts = (uint32_t)(now_ts - sender->first_ts);

		/*将发送记录送入拥塞对象中进行bwe对象做延迟估算*/
		sender->cc->on_send(sender->cc, fec_packet->transport_seq, fec_packet->fec_data_size + SIM_SEGMENT_HEADER_SIZE);

		INIT_SIM_HEADER(header, SIM_FEC, s->uid);
		sim_encode_msg(&s->sstrm, &header, fec_packet);

		sim_session_network_send(s, &s->sstrm);
	}
}

void free_video_seg(skiplist_item_t key, skiplist_item_t val, void* args)
{
	if (val.ptr != NULL)
		free(val.ptr);
}

sim_sender_t* sim_sender_create(sim_session_t* s, int transport_type, int padding, int fec)
{
	int cc_type;
	sim_sender_t* sender = calloc(1, sizeof(sim_sender_t));
	sender->first_ts = -1;

	sender->fecs_cache = skiplist_create(idu32_compare, free_video_seg, s);
	sender->segs_cache = skiplist_create(idu32_compare, free_video_seg, s);
	sender->ack_cache = skiplist_create(idu32_compare, NULL, s);
	/*pacer queue的延迟不大于250ms*/
	cc_type = transport_type;
	if (cc_type < gcc_transport || cc_type > remb_transport)
		cc_type = gcc_congestion;

	sender->cc = razor_sender_create(cc_type, padding, s, sim_bitrate_change, sender, sim_send_packet, 1000);

	sender->s = s;

	if (fec == 1)
		sender->flex = flex_fec_sender_create();

	sender->out_fecs = create_list();

	return sender;
}

void sim_sender_destroy(sim_session_t* s, sim_sender_t* sender)
{
	if (sender == NULL)
		return;

	if (sender->fecs_cache != NULL){
		skiplist_destroy(sender->fecs_cache);
		sender->fecs_cache = NULL;
	}

	if (sender->segs_cache != NULL){
		skiplist_destroy(sender->segs_cache);
		sender->segs_cache = NULL;
	}

	if (sender->ack_cache != NULL){
		skiplist_destroy(sender->ack_cache);
		sender->ack_cache = NULL;
	}

	if (sender->cc != NULL){
		razor_sender_destroy(sender->cc);
		sender->cc = NULL;
	}

	if (sender->flex != NULL){
		flex_fec_sender_destroy(sender->flex);
		sender->flex = NULL;
	}

	if (sender->out_fecs != NULL){
		destroy_list(sender->out_fecs);
		sender->out_fecs = NULL;
	}

	free(sender);
}

void sim_sender_reset(sim_session_t* s, sim_sender_t* sender, int transport_type, int padding, int fec)
{
	int cc_type;

	sender->actived = 0;
	sender->base_packet_id = 0;
	sender->send_id_seed = 0;
	sender->packet_id_seed = 0;
	sender->frame_id_seed = 0;

	sender->first_ts = -1;
	sender->transport_seq_seed = 0;

	skiplist_clear(sender->fecs_cache);
	skiplist_clear(sender->segs_cache);
	skiplist_clear(sender->ack_cache);

	list_clear(sender->out_fecs);

	/*重置拥塞对象*/
	if (sender->cc != NULL){
		razor_sender_destroy(sender->cc);
		sender->cc = NULL;
	}

	if (sender->flex){
		flex_fec_sender_destroy(sender->flex);
		sender->flex = NULL;
	}

	cc_type = transport_type;
	if (cc_type < gcc_transport || cc_type > remb_transport)
		cc_type = gcc_congestion;

	sender->cc = razor_sender_create(cc_type, padding, s, sim_bitrate_change, sender, sim_send_packet, 300);

	if (fec == 1)
		sender->flex = flex_fec_sender_create();
}

int sim_sender_active(sim_session_t* s, sim_sender_t* sender)
{
	if (sender->actived == 1)
		return -1;

	sender->actived = 1;
	return 0;
}

/*视频分片*/
#define SPLIT_NUMBER	1024
static uint16_t sim_split_frame(sim_session_t* s, uint16_t splits[], size_t size, int segment_size)
{
	uint16_t ret, i;
	uint16_t remain_size, packet_size;

	if (size <= segment_size){
		ret = 1;
		splits[0] = size;
	}
	else{
		ret = (size + segment_size - 1) / segment_size;
		packet_size = size / ret;
		remain_size = size % ret;

		for (i = 0; i < ret; i++){
			if (remain_size > 0){
				splits[i] = packet_size + 1;
				remain_size--;
			}
			else
				splits[i] = packet_size;
		}
	}

	return ret;
}

static void sim_sender_fec(sim_session_t* s, sim_sender_t* sender)
{
	skiplist_item_t key, val;
	sim_fec_t* fec;

	flex_fec_sender_update(sender->flex, s->loss_fraction, sender->out_fecs);

	while (list_size(sender->out_fecs) > 0){
		fec = list_pop(sender->out_fecs);
		if (fec != NULL){
			key.u32 = ++sender->send_id_seed;
			val.ptr = fec;
			skiplist_insert(sender->fecs_cache, key, val);
			fec->send_ts = (uint32_t)(GET_SYS_MS() - sender->first_ts);

			sender->cc->add_packet(sender->cc, key.u32, 1, fec->fec_data_size + SIM_SEGMENT_HEADER_SIZE);
		}
	}
}

int sim_sender_put(sim_session_t* s, sim_sender_t* sender, uint8_t payload_type, uint8_t ftype, const uint8_t* data, size_t size)
{
	sim_segment_t* seg;
	int64_t now_ts;
	uint16_t splits[SPLIT_NUMBER], total, i;
	uint8_t* pos;
	skiplist_item_t key, val;
	uint32_t timestamp;
	int segment_size;

	assert((size / SIM_VIDEO_SIZE) < SPLIT_NUMBER);
	if (ftype == 1)
		sim_debug("sender put video frame, data size = %d\n", size);
	now_ts = GET_SYS_MS();
	/*帧分包*/
	segment_size = SIM_VIDEO_SIZE / 2;
	total = sim_split_frame(s, splits, size, segment_size);

	/*计算时间戳*/
	if (sender->first_ts == -1){
		timestamp = 0;
		sender->first_ts = now_ts;
	}
	else
		timestamp = (uint32_t)(now_ts - sender->first_ts);

	pos = (uint8_t*)data;
	++sender->frame_id_seed;
	for (i = 0; i < total; ++i){
		seg = malloc(sizeof(sim_segment_t));

		seg->packet_id = ++sender->packet_id_seed;
		seg->send_id = ++sender->send_id_seed;
		seg->fid = sender->frame_id_seed;
		seg->timestamp = timestamp;
		seg->ftype = ftype;
		seg->payload_type = payload_type;
		seg->index = i;
		seg->total = total;

		seg->remb = 1;
		seg->fec_id = 0;
		seg->send_ts = 0;
		seg->transport_seq = 0;

		seg->data_size = splits[i];
		memcpy(seg->data, pos, seg->data_size);
		pos += splits[i];

		if (sender->flex != NULL){
			seg->fec_id = sender->flex->fec_id;					/*确定fec id*/
			flex_fec_sender_add_segment(sender->flex, seg);
		}

		/*将报文加入到发送缓冲队列当中*/
		key.u32 = seg->send_id;
		val.ptr = seg;
		skiplist_insert(sender->segs_cache, key, val);

		key.u32 = seg->packet_id;
		skiplist_insert(sender->ack_cache, key, val);

		/*将报文加入到cc的pacer中*/
		sender->cc->add_packet(sender->cc, seg->send_id, 0, seg->data_size + SIM_SEGMENT_HEADER_SIZE);

		if (sender->flex != NULL && sender->flex->segs_count >= 100)
			sim_sender_fec(s, sender);
	}
	if (sender->flex != NULL)
		sim_sender_fec(s, sender);

	return 0;
}

static inline void sim_sender_update_base(sim_session_t* s, sim_sender_t* sender, uint32_t base_packet_id)
{
	if (base_packet_id > sender->base_packet_id)
		sender->base_packet_id = base_packet_id;
}

/*处理nack消息*/
int sim_sender_ack(sim_session_t* s, sim_sender_t* sender, sim_segment_ack_t* ack)
{
	int i;
	sim_segment_t* seg;
	skiplist_iter_t* iter;
	skiplist_item_t key;

	int64_t now_ts;

	/*检查非法数据*/
	if (ack->acked_packet_id > sender->packet_id_seed || ack->base_packet_id > sender->packet_id_seed)
		return -1;
	/*推进窗口*/
	sim_sender_update_base(s, sender, ack->base_packet_id);

	now_ts = GET_SYS_MS();

	for (i = 0; i < ack->nack_num; ++i){
		key.u32 = ack->base_packet_id + ack->nack[i];
		if (sender->base_packet_id >= key.u32)
			continue;

		iter = skiplist_search(sender->ack_cache, key);
		if (iter != NULL){
			seg = (sim_segment_t*)iter->val.ptr;

			/*防止单个报文补偿过快,防止突发性拥塞*/
			if (seg->timestamp + seg->send_ts + sender->first_ts + SU_MIN(200, SU_MAX(30, s->rtt / 4)) > now_ts)
				continue;

			/*将报文加入到cc的pacer中进行重发*/
			sender->cc->add_packet(sender->cc, seg->send_id, 0, seg->data_size + SIM_SEGMENT_HEADER_SIZE);
		}
	}

	/*计算RTT*/
	key.u32 = ack->acked_packet_id;
	iter = skiplist_search(sender->ack_cache, key);
	if (iter != NULL){
		seg = (sim_segment_t*)iter->val.ptr;
		if (now_ts > seg->timestamp + seg->send_ts + sender->first_ts)
			sim_session_calculate_rtt(s, (uint16_t)(now_ts - seg->timestamp - seg->send_ts - sender->first_ts));
	}

	return 0;
}

/*处理接收端来的feedback消息*/
void sim_sender_feedback(sim_session_t* s, sim_sender_t* sender, sim_feedback_t* feedback)
{
	sim_sender_update_base(s, s->sender, feedback->base_packet_id);

	if (sender->cc != NULL)
		sender->cc->on_feedback(sender->cc, feedback->feedback, feedback->feedback_size);
}

void sim_sender_update_rtt(sim_session_t* s, sim_sender_t* sender)
{
	if (sender->cc != NULL){
		sender->cc->update_rtt(sender->cc, s->rtt + s->rtt_var);
	}
}

void sim_sender_set_bitrates(sim_session_t* s, sim_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	if (sender->cc != NULL){
		sender->cc->set_bitrates(sender->cc, min_bitrate, start_bitrate, max_bitrate);
	}
}

#define MAX_CACHE_DELAY 6000
static void sim_sender_evict_cache(sim_session_t* s, sim_sender_t* sender, int64_t now_ts)
{
	skiplist_item_t key;
	skiplist_iter_t* iter;
	sim_segment_t* seg;
	sim_fec_t* fec;

	/*进行帧分片淘汰*/
	while (skiplist_size(sender->segs_cache) > 0){
		iter = skiplist_first(sender->segs_cache);
		seg = iter->val.ptr;
		if (seg->timestamp + sender->first_ts + MAX_CACHE_DELAY <= now_ts){
			key.u32 = seg->packet_id;
			skiplist_remove(sender->ack_cache, key);

			skiplist_remove(sender->segs_cache, iter->key);
		}
		else
			break;
	}

	while (skiplist_size(sender->fecs_cache) > 0){
		iter = skiplist_first(sender->fecs_cache);
		fec = iter->val.ptr;
		if (fec->send_ts + sender->first_ts + MAX_CACHE_DELAY / 3 < now_ts)
			skiplist_remove(sender->fecs_cache, iter->key);
		else
			break;
	}
}

void sim_sender_timer(sim_session_t* s, sim_sender_t* sender, uint64_t now_ts)
{
	if (sender->cc != NULL)
		sender->cc->heartbeat(sender->cc);

	sim_sender_evict_cache(s, sender, now_ts);
}

