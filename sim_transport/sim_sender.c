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
	if (s->loss_fraction == 0)
		s->loss_fraction = fraction_loss;
	else
		s->loss_fraction = (s->loss_fraction * 3 + fraction_loss) / 4;

	loss = s->loss_fraction / 255.0;
	/*todo:通过丢包率计算FEC比例，FEC机制在这进行计算！！！！*/

	/*留出7%码率做nack和feedback*/
	if (loss > 0.5) /*重传的带宽不能大于半*/
		loss = 0.5;

	/*计算视频编码器的码率,单位kbps*/
	video_bitrate_kbps = (uint32_t)((1.0 - loss) * payload_bitrate) / 1000;

	sim_info("loss = %f, bitrate = %u, video_bitrate_kbps = %u\n", loss, bitrate, video_bitrate_kbps);
	/*通知上层进行码率调整*/
	s->change_bitrate_cb(s->event, video_bitrate_kbps);

	/*设置重发最大的码率*/
	if (sender != NULL)
		sim_limiter_set_max_bitrate(&sender->limiter, bitrate / 8);
}

static void sim_send_packet(void* handler, uint32_t packet_id, int retrans, size_t size, int padding)
{
	sim_header_t header;
	sim_segment_t* seg;
	skiplist_iter_t* it;
	skiplist_item_t key;
	int64_t now_ts;

	sim_sender_t* sender = (sim_sender_t*)handler;
	sim_session_t* s = sender->s;
	sim_pad_t pad;

	now_ts = GET_SYS_MS();

	if (padding == 1){
		pad.transport_seq = sender->transport_seq_seed++;
		pad.send_ts = (uint16_t)(now_ts - sender->first_ts);
		pad.data_size = SU_MIN(size, SIM_VIDEO_SIZE);

		/*将发送记录送入拥塞对象中进行bwe对象做延迟估算*/
		sender->cc->on_send(sender->cc, pad.transport_seq, pad.data_size + SIM_SEGMENT_HEADER_SIZE);

		INIT_SIM_HEADER(header, SIM_PAD, s->uid);
		sim_encode_msg(&s->sstrm, &header, &pad);

		sim_session_network_send(s, &s->sstrm);
	}
	else{
		key.u32 = packet_id;
		it = skiplist_search(sender->cache, key);
		if (it == NULL){
			sim_debug("send packet to network failed, packet id = %u\n", packet_id);
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
}

void free_video_seg(skiplist_item_t key, skiplist_item_t val, void* args)
{
	sim_segment_t* seg = val.ptr;
	if (seg != NULL)
		free(seg);
}

sim_sender_t* sim_sender_create(sim_session_t* s, int transport_type, int padding)
{
	int cc_type;
	sim_sender_t* sender = calloc(1, sizeof(sim_sender_t));
	sender->first_ts = -1;

	sender->cache = skiplist_create(idu32_compare, free_video_seg, s);
	/*pacer queue的延迟不大于250ms*/
	cc_type = (transport_type == bbr_transport ? bbr_congestion : gcc_congestion);
	sender->cc = razor_sender_create(cc_type, padding, s, sim_bitrate_change, sender, sim_send_packet, 1000);
	
	sim_limiter_init(&sender->limiter, 300);

	sender->s = s;

	return sender;
}

void sim_sender_destroy(sim_session_t* s, sim_sender_t* sender)
{
	if (sender == NULL)
		return;

	if (sender->cache != NULL){
		skiplist_destroy(sender->cache);
		sender->cache = NULL;
	}

	if (sender->cc != NULL){
		razor_sender_destroy(sender->cc);
		sender->cc = NULL;
	}

	sim_limiter_destroy(&sender->limiter);

	free(sender);
}

void sim_sender_reset(sim_session_t* s, sim_sender_t* sender, int transport_type, int padding)
{
	int cc_type;

	sender->actived = 0;
	sender->base_packet_id = 0;
	sender->packet_id_seed = 0;
	sender->frame_id_seed = 0;

	sender->first_ts = -1;
	sender->transport_seq_seed = 0;

	skiplist_clear(sender->cache);

	/*重置拥塞对象*/
	if (sender->cc != NULL){
		razor_sender_destroy(sender->cc);
		sender->cc = NULL;
	}

	cc_type = (transport_type == bbr_transport ? bbr_congestion : gcc_congestion);
	sender->cc = razor_sender_create(cc_type, padding, s, sim_bitrate_change, sender, sim_send_packet, 300);
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
static uint16_t sim_split_frame(sim_session_t* s, uint16_t splits[], size_t size)
{
	uint16_t ret, i;
	uint16_t remain_size;

	if (size <= SIM_VIDEO_SIZE){
		ret = 1;
		splits[0] = size;
	}
	else{
		ret = size / SIM_VIDEO_SIZE;
		for (i = 0; i < ret; i++)
			splits[i] = SIM_VIDEO_SIZE;

		remain_size = size % SIM_VIDEO_SIZE;
		if (remain_size > 0){
			splits[ret] = remain_size;
			ret++;
		}
	}

	return ret;
}

int sim_sender_put(sim_session_t* s, sim_sender_t* sender, uint8_t payload_type, uint8_t ftype, const uint8_t* data, size_t size)
{
	sim_segment_t* seg;
	int64_t now_ts;
	uint16_t splits[SPLIT_NUMBER], total, i;
	uint8_t* pos;
	skiplist_item_t key, val;
	uint32_t timestamp;

	assert((size / SIM_VIDEO_SIZE) < SPLIT_NUMBER);

	now_ts = GET_SYS_MS();
	/*帧分包*/
	total = sim_split_frame(s, splits, size);

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
		seg->fid = sender->frame_id_seed;
		seg->timestamp = timestamp;
		seg->ftype = ftype;
		seg->payload_type = payload_type;
		seg->index = i;
		seg->total = total;

		seg->remb = 1;
		seg->send_ts = 0;
		seg->transport_seq = 0;

		seg->data_size = splits[i];
		memcpy(seg->data, pos, seg->data_size);
		pos += splits[i];

		/*将报文加入到发送缓冲队列当中*/
		key.u32 = seg->packet_id;
		val.ptr = seg;
		skiplist_insert(sender->cache, key, val);

		/*将报文加入到cc的pacer中*/
		sender->cc->add_packet(sender->cc, seg->packet_id, 0, seg->data_size + SIM_SEGMENT_HEADER_SIZE);
		/*sim_debug("cc add packet, packet id = %u\n", seg->packet_id);*/
	}

	return 0;
}

static inline void sim_sender_update_base(sim_session_t* s, sim_sender_t* sender, uint32_t base_packet_id)
{
	uint32_t i;
	skiplist_item_t key;

	for (i = sender->base_packet_id; i <= base_packet_id; ++i){
		key.u32 = i;
		skiplist_remove(sender->cache, key);
		/*sim_debug("sim sender remove packet id = %u\n", i);*/
	}

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

	sim_limiter_update(&sender->limiter, 0, now_ts);

	for (i = 0; i < ack->nack_num; ++i){
		key.u32 = ack->base_packet_id + ack->nack[i];
		iter = skiplist_search(sender->cache, key);
		if (iter != NULL){
			seg = (sim_segment_t*)iter->val.ptr;

			/*将报文加入到cc的pacer中进行重发*/
			if (sim_limiter_try(&sender->limiter, seg->data_size + SIM_SEGMENT_HEADER_SIZE, now_ts) == 0
				&& sender->cc->add_packet(sender->cc, seg->packet_id, 1, seg->data_size + SIM_SEGMENT_HEADER_SIZE) == 0){
				/*发送成功，将发送的字节数据加入到限制器当中*/
				sim_limiter_update(&sender->limiter, seg->data_size + SIM_SEGMENT_HEADER_SIZE, now_ts);
			}
		}
	}

	/*计算RTT*/
	now_ts = GET_SYS_MS();

	key.u32 = ack->acked_packet_id;
	iter = skiplist_search(sender->cache, key);
	if (iter != NULL){
		seg = (sim_segment_t*)iter->val.ptr;
		if (now_ts > seg->timestamp + seg->send_ts + sender->first_ts)
			sim_session_calculate_rtt(s, (uint16_t)(now_ts - seg->timestamp - seg->send_ts - sender->first_ts));
		skiplist_remove(sender->cache, key);
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

void sim_sender_timer(sim_session_t* s, sim_sender_t* sender, uint64_t cur_ts)
{
	if (sender->cc != NULL)
		sender->cc->heartbeat(sender->cc);
}

