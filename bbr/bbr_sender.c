/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_sender.h"
#include "razor_log.h"

#define k_min_pace_bitrate (10*1000)
#define k_bbr_heartbeat_timer 1000

bbr_sender_t* bbr_sender_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms, int padding)
{
	bbr_sender_t* s = calloc(1, sizeof(bbr_sender_t));
	s->bbr = NULL;
	s->trigger = trigger;
	s->trigger_cb = bitrate_cb;

	s->encoding_rate_ratio = 1.0f;
	s->notify_ts = -1;

	s->info.congestion_window = -1;
	s->target_bitrate = k_min_pace_bitrate;


	bbr_feedback_adapter_init(&s->feedback);

	s->pacer = bbr_pacer_create(handler, send_cb, queue_ms, padding);
	
	bbr_pacer_set_bitrate_limits(s->pacer, k_min_pace_bitrate);
	bbr_pacer_set_estimate_bitrate(s->pacer, k_min_pace_bitrate);

	bin_stream_init(&s->strm);

	return s;
}

void bbr_sender_destroy(bbr_sender_t* s)
{
	if (s == NULL)
		return;

	if (s->pacer != NULL){
		bbr_pacer_destroy(s->pacer);
		s->pacer = NULL;
	}

	bbr_feedback_adapter_destroy(&s->feedback);

	if (s->bbr != NULL){
		bbr_destroy(s->bbr);
		s->bbr = NULL;
	}

	bin_stream_destroy(&s->strm);

	free(s);
}

static void bbr_on_network_invalidation(bbr_sender_t* s)
{
	size_t outstanding;
	double fill;
	uint8_t loss;

	uint32_t pacing_rate_kbps, target_rate_bps, pading_rate_kbps, instant_rate_kbps;
	int acked_bitrate;
	if (s->info.congestion_window <= 0)
		return;

	/*设置pace参数*/
	pacing_rate_kbps = bbr_pacer_data_rate(&s->info.pacer_config);
	pading_rate_kbps = bbr_pacer_pad_rate(&s->info.pacer_config);
	/*计算反馈带宽*/
	outstanding = bbr_feedback_get_in_flight(&s->feedback);

	bbr_pacer_update_outstanding(s->pacer, outstanding);

	instant_rate_kbps = s->info.congestion_window / s->info.target_rate.rtt;
	target_rate_bps = (SU_MIN(s->info.target_rate.target_rate, instant_rate_kbps) * 8000);
	acked_bitrate = bbr_feedback_get_birate(&s->feedback);

	fill = 1.0 * outstanding / s->info.congestion_window;
	/*如果拥塞窗口满了，进行带宽递减*/
	if (fill > 1.0){
		s->encoding_rate_ratio = 0.9f;
		if (acked_bitrate > 0)
			s->target_bitrate = acked_bitrate * s->encoding_rate_ratio;
		else
			s->target_bitrate = s->target_bitrate * s->encoding_rate_ratio;
	}
	else {
		s->encoding_rate_ratio = 1;
		if (fill < 0.9)
			s->target_bitrate = s->target_bitrate + SU_MIN(32 * 1000, SU_MAX(s->min_bitrate / 32, 4 * 8000));
	}

	bbr_pacer_set_pacing_rate(s->pacer, pacing_rate_kbps * 8);

	s->target_bitrate = SU_MIN(target_rate_bps, s->target_bitrate);
	s->target_bitrate = SU_MAX(s->target_bitrate, s->min_bitrate);
	s->target_bitrate = SU_MIN(s->max_bitrate, s->target_bitrate);

	loss = /*(uint8_t)(s->info.target_rate.loss_rate_ratio * 255 + 0.5f);*/s->loss_fraction;

	if (pading_rate_kbps != 0)
		bbr_pacer_set_padding_rate(s->pacer, SU_MIN(target_rate_bps / 1000, pading_rate_kbps * 8));
	else
		bbr_pacer_set_padding_rate(s->pacer, 0);

	razor_debug("target = %u kbps, acked_birate = %dkbps, pacing = %u kbps, instant = %u kbps, loss = %u, congestion_window = %u, outstanding = %u, ratio = %2f, rtt = %lld\n\n", 
		s->target_bitrate / 8000, acked_bitrate / 8000, pacing_rate_kbps, instant_rate_kbps, loss,
		s->info.congestion_window, outstanding, s->encoding_rate_ratio, s->info.target_rate.rtt);

	/*如果数据发生变化，进行触发一个通信层通知*/
	if (s->target_bitrate != s->last_bitrate_bps || loss != s->last_fraction_loss){
		s->last_bitrate_bps = s->target_bitrate;
		s->last_rtt = (uint32_t)s->info.target_rate.rtt;
		s->last_fraction_loss = loss;

		if (s->trigger != NULL && s->trigger_cb != NULL)
			s->trigger_cb(s->trigger, s->target_bitrate, loss, (uint32_t)s->info.target_rate.rtt);
	}
}

void bbr_sender_heartbeat(bbr_sender_t* s, int64_t now_ts)
{
	bbr_pacer_try_transmit(s->pacer, now_ts);

	if (s->bbr != NULL && s->notify_ts + k_bbr_heartbeat_timer < now_ts){
		s->info = bbr_on_heartbeat(s->bbr, now_ts);
		bbr_on_network_invalidation(s);
		s->notify_ts = now_ts;
	}
}

int bbr_sender_add_pace_packet(bbr_sender_t* s, uint32_t packet_id, int retrans, size_t size)
{
	if (s->bbr == NULL)
		return -1;

	return bbr_pacer_insert_packet(s->pacer, packet_id, retrans, size, GET_SYS_MS());
}

void bbr_sender_send_packet(bbr_sender_t* s, uint16_t seq, size_t size)
{
	bbr_packet_info_t info;
	bbr_feedback_add_packet(&s->feedback, seq, size, &info);
	if (s->bbr != NULL){
		bbr_on_send_packet(s->bbr, &info);
		/*bbr_on_network_invalidation(s);*/
	}
} 

void bbr_sender_on_feedback(bbr_sender_t* s, uint8_t* feedback, int feedback_size)
{
	bbr_feedback_msg_t msg;
	uint32_t acked_bitrate;

	if (feedback_size <= 0)
		return;

	bin_stream_resize(&s->strm, feedback_size);
	bin_stream_rewind(&s->strm, 1);
	memcpy(s->strm.data, feedback, feedback_size);
	bin_stream_set_used_size(&s->strm, feedback_size);

	bbr_feedback_msg_decode(&s->strm, &msg);

	if (msg.flag & bbr_loss_info_msg == bbr_loss_info_msg)
		s->loss_fraction = msg.fraction_loss;

	bbr_feedback_on_feedback(&s->feedback, &msg);
	if (s->feedback.feedback.packets_num <= 0)
		return;

	if (s->bbr != NULL){
		acked_bitrate = bbr_feedback_get_birate(&s->feedback) / 8000;
		s->info = bbr_on_feedback(s->bbr, &s->feedback.feedback, acked_bitrate);
		bbr_on_network_invalidation(s);

		s->notify_ts = GET_SYS_MS();
	}
}

void bbr_sender_update_rtt(bbr_sender_t* s, int32_t rtt)
{

}

void bbr_sender_set_bitrates(bbr_sender_t* s, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	bbr_target_rate_constraint_t co;

	s->max_bitrate = max_bitrate;
	s->min_bitrate = min_bitrate;

	s->target_bitrate = start_bitrate;

	bbr_pacer_set_estimate_bitrate(s->pacer, start_bitrate);
	bbr_pacer_set_bitrate_limits(s->pacer, min_bitrate);

	if (s->bbr == NULL){
		co.at_time = GET_SYS_MS();
		co.min_rate = min_bitrate / 8000;
		co.max_rate = max_bitrate / 8000;

		s->bbr = bbr_create(&co, start_bitrate / 8000);
	}
}

int64_t bbr_sender_get_pacer_queue_ms(bbr_sender_t* s)
{
	return bbr_pacer_queue_ms(s->pacer);
}

int64_t bbr_sender_get_first_packet_ts(bbr_sender_t* s)
{
	return -1;
}

