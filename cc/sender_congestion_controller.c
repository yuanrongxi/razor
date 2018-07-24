/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sender_congestion_controller.h"
#include "razor_log.h"

#define k_max_queue_ms		250
#define k_min_bitrate_bps	10000

#define k_max_feedback_size 1472

static void sender_cc_on_change_bitrate(void* handler, uint32_t bitrate, uint8_t fraction_loss, uint32_t rtt)
{
	sender_cc_t* cc = (sender_cc_t*)handler;

	razor_info("sender change bitrate, bitrate = %ubps\n", bitrate);

	pace_set_estimate_bitrate(cc->pacer, bitrate);
	/*触发一个通信层通知*/
	if (cc->trigger != NULL && cc->trigger_cb != NULL)
		cc->trigger_cb(cc->trigger, bitrate, fraction_loss, rtt);
}

sender_cc_t* sender_cc_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms)
{
	sender_cc_t* cc = calloc(1, sizeof(sender_cc_t));
	cc->was_in_alr = -1;
	cc->accepted_queue_ms = SU_MIN(queue_ms, k_max_queue_ms);

	cc->rtt = 200;

	cc->trigger = trigger;
	cc->trigger_cb = bitrate_cb;

	cc->ack = ack_estimator_create();
	cc->bwe = delay_bwe_create();
	cc->pacer = pace_create(handler, send_cb, cc->accepted_queue_ms);

	feedback_adapter_init(&cc->adapter);

	delay_bwe_set_min_bitrate(cc->bwe, k_min_bitrate_bps);
	pace_set_bitrate_limits(cc->pacer, k_min_bitrate_bps);

	bin_stream_init(&cc->strm);

	cc->bitrate_controller = bitrate_controller_create(cc, sender_cc_on_change_bitrate);
	razor_info("create razor's sender cc!\n");

	return cc;
}

void sender_cc_destroy(sender_cc_t* cc)
{
	if (cc == NULL)
		return;

	razor_info("destroy razor's sender\n");

	if (cc->ack != NULL){
		ack_estimator_destroy(cc->ack);
		cc->ack = NULL;
	}

	if (cc->bitrate_controller != NULL){
		bitrate_controller_destroy(cc->bitrate_controller);
		cc->bitrate_controller = NULL;
	}

	if (cc->bwe != NULL){
		delay_bwe_destroy(cc->bwe);
		cc->bwe = NULL;
	}

	if (cc->pacer != NULL){
		pace_destroy(cc->pacer);
		cc->pacer = NULL;
	}

	feedback_adapter_destroy(&cc->adapter);

	bin_stream_destroy(&cc->strm);

	free(cc);

	razor_info("free razor's sender OK\n");
}

void sender_cc_heartbeat(sender_cc_t* cc)
{
	int64_t now_ts = GET_SYS_MS();
	/*进行pace发送*/
	pace_try_transmit(cc->pacer, now_ts);

	/*进行带宽调节*/
	bitrate_controller_heartbeat(cc->bitrate_controller, now_ts, ack_estimator_bitrate_bps(cc->ack));
}

int sender_cc_add_pace_packet(sender_cc_t* cc, uint32_t packet_id, int retrans, size_t size)
{
	return pace_insert_packet(cc->pacer, packet_id, retrans, size, GET_SYS_MS());
}

void sender_on_send_packet(sender_cc_t* cc, uint16_t seq, size_t size)
{
	feedback_add_packet(&cc->adapter, seq, size);

	/*todo:进行RTT周期内是否发送码率溢出，可以不实现*/
}

void sender_on_feedback(sender_cc_t* cc, uint8_t* feedback, int feedback_size)
{
	int cur_alr;
	bwe_result_t bwe_result;
	int64_t now_ts;
	feedback_msg_t msg;

	if (feedback_size > k_max_feedback_size)
		return;

	bin_stream_resize(&cc->strm, feedback_size);
	bin_stream_rewind(&cc->strm, 1);
	memcpy(cc->strm.data, feedback, feedback_size);
	cc->strm.used = feedback_size;

	/*解码得到反馈序列*/
	feedback_msg_decode(&cc->strm, &msg);

	now_ts = GET_SYS_MS();

	/*处理proxy estimate的信息*/
	if ((msg.flag & proxy_ts_msg) == proxy_ts_msg){
		if (feedback_on_feedback(&cc->adapter, &msg) <= 0)
			return;

		cur_alr = pace_get_limited_start_time(cc->pacer) > 0 ? 0 : -1;
		if (cc->was_in_alr == 0 && cur_alr != 0){
			ack_estimator_set_alrended(cc->ack, now_ts);
		}
		cc->was_in_alr = cur_alr;

		/*进行远端接收码率评估*/
		ack_estimator_incoming(cc->ack, cc->adapter.packets, cc->adapter.num);

		/*根据延迟状态进行发送端拥塞控制判断,并评估最新的发送码率*/
		init_bwe_result_null(bwe_result);
		bwe_result = delay_bwe_incoming(cc->bwe, cc->adapter.packets, cc->adapter.num, ack_estimator_bitrate_bps(cc->ack), now_ts);

		/*进行码率调节*/
		if (bwe_result.updated == 0)
			bitrate_controller_on_basedelay_result(cc->bitrate_controller, bwe_result.updated, bwe_result.probe, bwe_result.bitrate, cc->bwe->detector->state);
	}
	/*处理remb*/
	if ((msg.flag & remb_msg) == remb_msg){
		razor_debug("sender remb = %ubps\n", msg.remb);
		bitrate_controller_on_remb(cc->bitrate_controller, msg.remb);
	}
	/*处理loss info*/
	if ((msg.flag & loss_info_msg) == loss_info_msg){
		razor_debug("sender receive loss info, fraction_loss = %u, packets_num = %u\n", msg.fraction_loss, msg.packet_num);
		bitrate_controller_on_report(cc->bitrate_controller, cc->rtt, now_ts, msg.fraction_loss, msg.packet_num, ack_estimator_bitrate_bps(cc->ack));
	}
}

void sender_cc_update_rtt(sender_cc_t* cc, int32_t rtt)
{
	cc->rtt = rtt;
	delay_bwe_rtt_update(cc->bwe, rtt);

	/*razor_debug("set razor sender rtt = %dms\n", rtt);*/
}

void sender_cc_set_bitrates(sender_cc_t* cc, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	start_bitrate = SU_MIN(SU_MAX(start_bitrate, min_bitrate), max_bitrate);

	delay_bwe_set_min_bitrate(cc->bwe, min_bitrate);
	delay_bwe_set_max_bitrate(cc->bwe, max_bitrate);

	delay_bwe_set_start_bitrate(cc->bwe, start_bitrate);

	bitrate_controller_set_bitrates(cc->bitrate_controller, start_bitrate, min_bitrate, max_bitrate);

	/*pace是用BYTE计算*/
	pace_set_bitrate_limits(cc->pacer, min_bitrate);
	pace_set_estimate_bitrate(cc->pacer, start_bitrate);


	razor_info("set razor sender bitrates, min = %ubps, max = %ubps, start = %ubps\n", min_bitrate, max_bitrate, start_bitrate);
}

int64_t sender_cc_get_pacer_queue_ms(sender_cc_t* cc)
{
	return pace_queue_ms(cc->pacer);
}

int64_t sender_cc_get_first_packet_ts(sender_cc_t* cc)
{
	return cc->pacer->first_sent_ts;
}





