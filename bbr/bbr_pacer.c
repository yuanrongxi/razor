/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_pacer.h"
#include "razor_log.h"

#define k_min_packet_limit_ms		5			/*发包最小间隔*/
#define k_max_packet_limit_ms		200			/*每400毫秒必须发送一个报文，防止反馈失败*/
#define k_max_interval_ms			30			/*发包最大时间差，长时间不发送报文一次发送很多数据出去造成网络风暴*/
#define k_default_pace_factor		1.25		/*默认的pace factor因子*/	


bbr_pacer_t* bbr_pacer_create(void* handler, pace_send_func send_cb, uint32_t que_ms, int padding)
{
	bbr_pacer_t* pace = calloc(1, sizeof(bbr_pacer_t));
	pace->last_update_ts = GET_SYS_MS();
	pace->handler = handler;
	pace->send_cb = send_cb;
	pace->padding = padding;
	pace->factor = k_default_pace_factor;
	pace->congestion_window_size = 0xffffffff;

	pacer_queue_init(&pace->que, que_ms);

	init_interval_budget(&pace->media_budget, 0, -1);
	increase_budget(&pace->media_budget, k_min_packet_limit_ms);

	init_interval_budget(&pace->padding_budget, 0, -1);
	increase_budget(&pace->padding_budget, k_min_packet_limit_ms);

	return pace;
}

void bbr_pacer_destroy(bbr_pacer_t* pace)
{
	if (pace == NULL)
		return;

	pacer_queue_destroy(&pace->que);

	free(pace);
}

void bbr_pacer_set_estimate_bitrate(bbr_pacer_t* pace, uint32_t bitrate_bps)
{
	pace->estimated_bitrate = bitrate_bps;
	pace->pacing_bitrate_kpbs = SU_MAX(bitrate_bps / 1000, pace->min_sender_bitrate_kpbs) * pace->factor;
	razor_debug("set pacer bitrate, bitrate = %ubps\n", bitrate_bps);
}

void bbr_pacer_set_bitrate_limits(bbr_pacer_t* pace, uint32_t min_bitrate)
{
	pace->min_sender_bitrate_kpbs = min_bitrate / 1000;
	pace->pacing_bitrate_kpbs = SU_MAX(pace->estimated_bitrate / 1000, pace->min_sender_bitrate_kpbs) * pace->factor;
	razor_info("set pacer min bitrate, bitrate = %ubps\n", min_bitrate);
}

void bbr_pacer_set_pacing_rate(bbr_pacer_t* pace, uint32_t pacing_bitrate_kbps)
{
	pace->pacing_bitrate_kpbs = pacing_bitrate_kbps* pace->factor;
	razor_debug("set pacer bitrate, bitrate = %ukbps\n", pacing_bitrate_kbps);
	//set_target_rate_kbps(&pace->padding_budget, pace->pacing_bitrate_kpbs);
} 

void bbr_pacer_set_padding_rate(bbr_pacer_t* pace, uint32_t padding_bitrate)
{
	set_target_rate_kbps(&pace->padding_budget, padding_bitrate);
}

void bbr_pacer_update_outstanding(bbr_pacer_t* pace, size_t outstanding_bytes)
{
	pace->outstanding_bytes = outstanding_bytes;
}

void bbr_pacer_update_congestion_window(bbr_pacer_t* pace, size_t congestion_window_size)
{
	pace->congestion_window_size = congestion_window_size;
}

void bbr_pacer_set_factor(bbr_pacer_t* pace, float factor)
{
	pace->factor = factor;
	bbr_pacer_set_estimate_bitrate(pace, pace->estimated_bitrate);
}

int bbr_pacer_insert_packet(bbr_pacer_t* pace, uint32_t seq, int retrans, size_t size, int64_t now_ts)
{
	packet_event_t ev;
	ev.seq = seq;
	ev.retrans = retrans;
	ev.size = size;
	ev.que_ts = now_ts;
	ev.sent = 0;
	return pacer_queue_push(&pace->que, &ev);
}

int64_t bbr_pacer_queue_ms(bbr_pacer_t* pace)
{
	return GET_SYS_MS() - pacer_queue_oldest(&pace->que);
}

size_t bbr_pacer_queue_size(bbr_pacer_t* pace)
{
	return pacer_queue_bytes(&pace->que);
}

static int bbr_pacer_congestion(bbr_pacer_t* pace)
{
	return pace->congestion_window_size <= pace->outstanding_bytes ? -1 : 0;
}

static int bbr_pacer_send(bbr_pacer_t* pace, packet_event_t* ev)
{
	/*进行发送控制*/
	if (budget_remaining(&pace->media_budget) == 0 || bbr_pacer_congestion(pace) == -1)
		return -1;

	/*调用外部接口进行数据发送*/
	if (pace->send_cb != NULL)
		pace->send_cb(pace->handler, ev->seq, ev->retrans, ev->size, 0);

	use_budget(&pace->media_budget, ev->size);
	use_budget(&pace->padding_budget, ev->size);
	pace->outstanding_bytes += ev->size;

	return 0;
}

#define PADDING_SIZE 500
void bbr_pacer_try_transmit(bbr_pacer_t* pace, int64_t now_ts)
{
	int elapsed_ms;
	uint32_t target_bitrate_kbps;
	int sent_bytes;
	packet_event_t* ev;

	elapsed_ms = (int)(now_ts - pace->last_update_ts);

	if (elapsed_ms < k_min_packet_limit_ms)
		return;

	/*如果拥塞了，每400毫秒必须发送一个包，防止feedback无法反馈*/
	if (elapsed_ms >= k_max_packet_limit_ms && bbr_pacer_congestion(pace) == -1){
		ev = pacer_queue_front(&pace->que);
		if (ev != NULL){
			if (pace->send_cb != NULL)
				pace->send_cb(pace->handler, ev->seq, ev->retrans, ev->size, 0);

			pace->outstanding_bytes += ev->size;
			pacer_queue_sent(&pace->que, ev);
		}

		pace->last_update_ts = now_ts;
	}
	else if (bbr_pacer_congestion(pace) == 0){
		elapsed_ms = SU_MIN(elapsed_ms, k_max_interval_ms);

		pace->last_update_ts = now_ts;
		/*计算media budget中需要的码率,并更新到media budget之中*/
		if (pacer_queue_bytes(&pace->que) > 0){
			target_bitrate_kbps = pacer_queue_target_bitrate_kbps(&pace->que, now_ts);
			target_bitrate_kbps = SU_MAX(pace->pacing_bitrate_kpbs, target_bitrate_kbps);
		}
		else
			target_bitrate_kbps = pace->pacing_bitrate_kpbs;
		/*从新计算可以发送的直接数量*/
		set_target_rate_kbps(&pace->media_budget, target_bitrate_kbps);
		increase_budget(&pace->media_budget, elapsed_ms);
		increase_budget(&pace->padding_budget, elapsed_ms);

		/*进行发包*/
		sent_bytes = 0;
		while (pacer_queue_empty(&pace->que) != 0){
			ev = pacer_queue_front(&pace->que);
			if (bbr_pacer_send(pace, ev) == 0)
				pacer_queue_sent(&pace->que, ev);
			else
				break;
		}

		/*检查是否可以padding*/
		while (pace->padding == 1 && budget_remaining(&pace->padding_budget) > PADDING_SIZE / 4){
			if (bbr_pacer_congestion(pace) == -1)
				break;
			
			if (pace->send_cb != NULL){
				pace->send_cb(pace->handler, 0, 0, PADDING_SIZE, 1);

				use_budget(&pace->media_budget, PADDING_SIZE);
				use_budget(&pace->padding_budget, PADDING_SIZE);
				pace->outstanding_bytes += PADDING_SIZE;
			}
			else 
				break;
		}
	}
}



