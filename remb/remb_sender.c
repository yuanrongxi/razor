#include "remb_sender.h"
#include "estimator_common.h"

#include "razor_log.h"

#define remb_max_queue_ms		500
#define remb_min_bitrate_bps	10000
#define remb_rate_wnd_size		2000
#define remb_rate_scale			8000

static void sender_remb_on_change_bitrate(remb_sender_t* cc, uint32_t bitrate, uint8_t fraction_loss, uint32_t rtt)
{
	razor_info("remb sender change bitrate, bitrate = %uKB/s\n", bitrate / 8000);

	bbr_pacer_set_estimate_bitrate(cc->pacer, bitrate);
	bbr_pacer_set_padding_rate(cc->pacer, bitrate / 1000);

	cc->pacer->factor = 1.25f;
	 
	/*触发一个通信层通知*/
	if (cc->trigger != NULL && cc->trigger_cb != NULL)
		cc->trigger_cb(cc->trigger, bitrate, fraction_loss, rtt);
}

static void remb_sender_update_rate(void* handler, int size)
{
	int64_t now_ts;

	remb_sender_t* sender = (remb_sender_t*)handler;
	if (sender != NULL){
		now_ts = GET_SYS_MS();
		rate_stat_update(&sender->rate, size, now_ts);
	}
}

remb_sender_t* remb_sender_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms, int padding)
{
	remb_sender_t* sender = calloc(1, sizeof(remb_sender_t));
	
	sender->trigger = trigger;
	sender->trigger_cb = bitrate_cb;

	sender->pacer = bbr_pacer_create(handler, send_cb, sender, remb_sender_update_rate, remb_max_queue_ms, padding);
	bbr_pacer_set_bitrate_limits(sender->pacer, remb_min_bitrate_bps);

	bin_stream_init(&sender->strm);
	rate_stat_init(&sender->rate, remb_rate_wnd_size, remb_rate_scale);

	sender->slope.var_rtt = sender->slope.prev_rtt = 0;

	return sender;
}

void remb_sender_destroy(remb_sender_t* s)
{
	if (s == NULL)
		return;

	if (s->pacer != NULL){
		bbr_pacer_destroy(s->pacer);
		s->pacer = NULL;
	}

	bin_stream_destroy(&s->strm);
	rate_stat_destroy(&s->rate);

	free(s);
}

void remb_sender_heartbeat(remb_sender_t* s, int64_t now_ts)
{
	bbr_pacer_try_transmit(s->pacer, now_ts);

	rate_stat_rate(&s->rate, now_ts);
}

int remb_sender_add_pace_packet(remb_sender_t* s, uint32_t packet_id, int retrans, size_t size)
{
	return bbr_pacer_insert_packet(s->pacer, packet_id, retrans, size, GET_SYS_MS());
}

void remb_sender_send_packet(remb_sender_t* s, uint16_t seq, size_t size)
{
}

void remb_sender_update_rtt(remb_sender_t* s, int32_t rtt)
{
	int32_t delta = 0;
	if (s->slope.prev_rtt == 0){
		s->slope.var_rtt = rtt;
		s->slope.prev_rtt = rtt;
		s->slope.slope = 0;
	}
	else{
		delta = rtt - s->slope.prev_rtt;

		s->slope.acc -= s->slope.frags[s->slope.index++ % DELAY_WND_SIZE];
		s->slope.frags[s->slope.index % DELAY_WND_SIZE] = delta;
		s->slope.acc += delta;

		if (s->slope.index > DELAY_WND_SIZE)
			s->slope.slope = s->slope.acc / DELAY_WND_SIZE;
		else if (s->slope.index > 4)
			s->slope.slope = s->slope.acc / s->slope.index;

		s->slope.var_rtt = (abs(delta) + s->slope.var_rtt * 3) / 4;

		s->slope.prev_rtt = rtt;

		if (s->slope.acc > SU_MAX(50, s->slope.var_rtt)){
			s->target_bitrate = s->target_bitrate * 7 / 8;
			s->target_bitrate = SU_MIN(s->max_bitrate, SU_MAX(s->min_bitrate, s->target_bitrate));
			sender_remb_on_change_bitrate(s, s->target_bitrate, s->loss_fraction, 0);
		}
	}
}

void remb_sender_on_feedback(remb_sender_t* s, uint8_t* feedback, int feedback_size)
{
	int64_t now_ts = GET_SYS_MS();
	int32_t bitrate;
	feedback_msg_t msg;

	bitrate = rate_stat_rate(&s->rate, now_ts);

	bin_stream_resize(&s->strm, feedback_size);
	bin_stream_rewind(&s->strm, 1);
	memcpy(s->strm.data, feedback, feedback_size);
	s->strm.used = feedback_size;

	feedback_msg_decode(&s->strm, &msg);

	if ((msg.flag & loss_info_msg) == loss_info_msg){
		s->loss_fraction = msg.fraction_loss;
	}

	/*确定REMB的带宽*/
	if ((msg.flag & remb_msg) == remb_msg){

		if (msg.remb <= 0)
			return;
		
		razor_debug("remb sender bitrates, remb = %dKB/s, target = %uKB/s, send rate = %uKB/s, loss fraction = %u\n", 
			msg.remb / 8000, s->target_bitrate/8000, bitrate/8000, s->loss_fraction);

		if (msg.remb > bitrate * 1.4142f || s->slope.acc > SU_MIN(50, s->slope.var_rtt))
				s->target_bitrate = SU_MIN(s->target_bitrate, msg.remb);
		else if (s->loss_fraction <= 0 && s->slope.prev_rtt < 1500)
			s->target_bitrate = SU_MAX(s->target_bitrate, msg.remb);
		else
			s->target_bitrate = msg.remb;

		if (s->slope.acc >  SU_MIN(50, s->slope.var_rtt)){
			s->target_bitrate = s->target_bitrate * 7 / 8;
		}
		else{
			if (s->loss_fraction < 2 && s->slope.acc <= 10 && s->slope.prev_rtt < 1500){
				s->target_bitrate = s->target_bitrate + SU_MAX(32000, SU_MIN(400000, s->target_bitrate / 32));
			}
		}

		s->target_bitrate = SU_MIN(s->max_bitrate, SU_MAX(s->min_bitrate, s->target_bitrate));

		sender_remb_on_change_bitrate(s, s->target_bitrate, s->loss_fraction, 0);
	}
}

void remb_sender_set_bitrates(remb_sender_t* s, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	start_bitrate = SU_MIN(SU_MAX(start_bitrate, min_bitrate), max_bitrate);

	bbr_pacer_set_bitrate_limits(s->pacer, min_bitrate);
	bbr_pacer_set_estimate_bitrate(s->pacer, start_bitrate);

	s->target_bitrate = start_bitrate;

	s->max_bitrate = max_bitrate;
	s->min_bitrate = min_bitrate;
}

int64_t remb_sender_get_pacer_queue_ms(remb_sender_t* s)
{
	return bbr_pacer_queue_ms(s->pacer);
}

int64_t remb_sender_get_first_packet_ts(remb_sender_t* s)
{
	return -1;
}



