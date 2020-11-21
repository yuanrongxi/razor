/*-
* Copyright (c) 2017-2019 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "remb_receiver.h"
#include "estimator_common.h"
#include "razor_log.h"

#define remb_rate_wnd_size		2000
#define remb_rate_scale			8000

#define remb_time_event			200

remb_receiver_t* remb_receive_create(void* handler, send_feedback_func cb)
{
	remb_receiver_t* r = calloc(1, sizeof(remb_receiver_t));
	r->feedback_ts = GET_SYS_MS();
	r->handler = handler;
	r->send_cb = cb;

	bin_stream_init(&r->strm);
	loss_statistics_init(&r->loss_stat);
	rate_stat_init(&r->rate, remb_rate_wnd_size, remb_rate_scale);

	return r;
}

void remb_receive_destroy(remb_receiver_t* r)
{
	if (r == NULL)
		return;

	bin_stream_destroy(&r->strm);
	loss_statistics_destroy(&r->loss_stat);
	rate_stat_destroy(&r->rate);
	
	free(r);
}

void remb_receive_heartbeat(remb_receiver_t* r)
{
	int64_t now_ts;
	feedback_msg_t msg;
	int32_t bitrate, num, rec_rate;
	uint8_t loss;

	now_ts = GET_SYS_MS();

	if (r->feedback_ts + remb_time_event <= now_ts){
		bitrate = rate_stat_rate(&r->rate, now_ts);
		if (bitrate <= 0)
			return;

		msg.flag |= remb_msg;
		msg.remb = bitrate;

		if (loss_statistics_calculate(&r->loss_stat, now_ts, &loss, &num) == 0){
			msg.flag |= loss_info_msg;
			msg.fraction_loss = loss;
			msg.packet_num = num;
		}

		msg.samples_num = 0;

		bin_stream_rewind(&r->strm, 1);
		feedback_msg_encode(&r->strm, &msg);

		r->send_cb(r->handler, r->strm.data, r->strm.used);

		r->feedback_ts = now_ts;
		razor_debug("remb receiver set bitrate = %uKB/s, rec rate = %dKB/S\n", bitrate / remb_rate_scale);
	}
}

void remb_receive_check_acked(remb_receiver_t* r)
{

}

void remb_receive_on_received(remb_receiver_t* r, uint16_t seq, uint32_t timestamp, size_t size, int remb)
{
	int64_t now_ts;
	now_ts = GET_SYS_MS();

	loss_statistics_incoming(&r->loss_stat, seq, now_ts);
	rate_stat_update(&r->rate, size, now_ts);
}

void remb_receive_update_rtt(remb_receiver_t* r, uint32_t rtt)
{

}

void remb_receive_set_min_bitrate(remb_receiver_t* r, int min_bitrate)
{
	r->min_bitrate = min_bitrate;
}

void remb_receive_set_max_bitrate(remb_receiver_t* r, int max_bitrate)
{
	r->max_bitrate = max_bitrate;
}



