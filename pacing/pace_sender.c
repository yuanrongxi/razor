/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "pace_sender.h"
#include "razor_log.h"

#define k_min_packet_limit_ms		5			/*发包最小间隔*/
#define k_max_interval_ms			50			/*发包最大时间差，长时间不发送报文一次发送很多数据出去造成网络风暴*/
#define k_default_pace_factor		1.5

pace_sender_t* pace_create(void* handler, pace_send_func send_cb, uint32_t que_ms)
{
	pace_sender_t* pace = calloc(1, sizeof(pace_sender_t));

	pace->first_sent_ts = -1;
	pace->last_update_ts = GET_SYS_MS();
	pace->handler = handler;
	pace->send_cb = send_cb;

	pace->alr = alr_detector_create();
	pacer_queue_init(&pace->que, que_ms);

	init_interval_budget(&pace->media_budget, 0, -1);
	increase_budget(&pace->media_budget, k_min_packet_limit_ms);

	return pace;
}

void pace_destroy(pace_sender_t* pace)
{
	if (pace == NULL)
		return;

	pacer_queue_destroy(&pace->que);

	if (pace->alr != NULL){
		alr_detector_destroy(pace->alr);
		pace->alr = NULL;
	}

	free(pace);
}

void pace_set_estimate_bitrate(pace_sender_t* pace, uint32_t bitrate_bps)
{
	pace->estimated_bitrate = bitrate_bps;
	pace->pacing_bitrate_kpbs = SU_MAX(bitrate_bps / 1000, pace->min_sender_bitrate_kpbs) * k_default_pace_factor;
	alr_detector_set_bitrate(pace->alr, bitrate_bps);

	razor_debug("set pacer bitrate, bitrate = %ubps\n", bitrate_bps);
}

/*设置最小带宽限制*/
void pace_set_bitrate_limits(pace_sender_t* pace, uint32_t min_sent_bitrate_pbs)
{
	pace->min_sender_bitrate_kpbs = min_sent_bitrate_pbs / 1000;
	pace->pacing_bitrate_kpbs = SU_MAX(pace->estimated_bitrate / 1000, pace->min_sender_bitrate_kpbs) * k_default_pace_factor;

	razor_info("set pacer min bitrate, bitrate = %ubps\n", min_sent_bitrate_pbs);
}

/*将一个即将要发送的报文放入排队队列中*/
int pace_insert_packet(pace_sender_t* pace, uint32_t seq, int retrans, size_t size, int64_t now_ts)
{
	packet_event_t ev;
	ev.seq = seq;
	ev.retrans = retrans;
	ev.size = size;
	ev.que_ts = now_ts;
	ev.sent = 0;
	return pacer_queue_push(&pace->que, &ev);
}

int64_t pace_queue_ms(pace_sender_t* pace)
{
	return GET_SYS_MS() - pacer_queue_oldest(&pace->que);
}

size_t pace_queue_size(pace_sender_t* pace)
{
	return pacer_queue_bytes(&pace->que);
}

int64_t pace_expected_queue_ms(pace_sender_t* pace)
{
	if (pace->pacing_bitrate_kpbs > 0)
		return pacer_queue_bytes(&pace->que) * 8 / pace->pacing_bitrate_kpbs;
	else
		return 0;
}

int64_t pace_get_limited_start_time(pace_sender_t* pace)
{
	return alr_get_app_limited_started_ts(pace->alr);
}

static int pace_send(pace_sender_t* pace, packet_event_t* ev)
{
	/*进行发送控制*/
	if (budget_remaining(&pace->media_budget) == 0)
		return -1;

	/*调用外部接口进行数据发送*/
	if (pace->send_cb != NULL)
		pace->send_cb(pace->handler, ev->seq, ev->retrans, ev->size, 0);

	use_budget(&pace->media_budget, ev->size);

	return 0;
}

void pace_try_transmit(pace_sender_t* pace, int64_t now_ts)
{
	int elapsed_ms;
	uint32_t target_bitrate_kbps;
	int sent_bytes;
	packet_event_t* ev;

	elapsed_ms = (int)(now_ts - pace->last_update_ts);

	if (elapsed_ms < k_min_packet_limit_ms)
		return;

	elapsed_ms = SU_MIN(elapsed_ms, k_max_interval_ms);
	
	/*计算media budget中需要的码率,并更新到media budget之中*/
	if (pacer_queue_bytes(&pace->que) > 0){
		target_bitrate_kbps = pacer_queue_target_bitrate_kbps(&pace->que, now_ts);
		target_bitrate_kbps = SU_MAX(pace->pacing_bitrate_kpbs, target_bitrate_kbps);
	}
	else
		target_bitrate_kbps = pace->pacing_bitrate_kpbs;

	set_target_rate_kbps(&pace->media_budget, target_bitrate_kbps);
	increase_budget(&pace->media_budget, elapsed_ms);

	/*进行发包*/
	sent_bytes = 0;
	while (pacer_queue_empty(&pace->que) != 0){
		ev = pacer_queue_front(&pace->que);
		if (pace_send(pace, ev) == 0){
			/*razor_debug("pace send packet, packet id = %u, pace cache size = %d, pace delay = %ums\n", ev->seq, pace->que.l->size, pace_queue_ms(pace));*/
			if (pace->first_sent_ts == -1)
				pace->first_sent_ts = now_ts;
			
			sent_bytes += ev->size;
			pacer_queue_sent(&pace->que, ev);
		}
		else
			break;
	}

	pace->last_update_ts = now_ts;
	if (sent_bytes > 0){
		/*更新预测器,假如预测期空闲的空间太多，进行加大码率*/
		alr_detector_bytes_sent(pace->alr, sent_bytes, elapsed_ms);
	}
}







