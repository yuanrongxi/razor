/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "razor_api.h"
#include "sender_congestion_controller.h"
#include "receiver_congestion_controller.h"
#include "bbr_sender.h"
#include "bbr_receiver.h"

static void razor_sender_heartbeat(razor_sender_t* sender)
{
	if (sender != NULL)
		sender_cc_heartbeat((sender_cc_t*)sender);
}

static void razor_bbr_sender_heartbeat(razor_sender_t* sender)
{
	int64_t now_ts = GET_SYS_MS();
	if (sender != NULL)
		bbr_sender_heartbeat((bbr_sender_t*)sender, now_ts);
}

static void razor_sender_set_bitrates(razor_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	if (sender != NULL)
		sender_cc_set_bitrates((sender_cc_t*)sender, min_bitrate, start_bitrate, max_bitrate);
}

static void razor_bbr_sender_set_bitrates(razor_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	if (sender != NULL)
		bbr_sender_set_bitrates((bbr_sender_t*)sender, min_bitrate, start_bitrate, max_bitrate);
}

static int razor_sender_add_packet(razor_sender_t* sender, uint32_t packet_id, int retrans, size_t size)
{
	if (sender != NULL)
		return sender_cc_add_pace_packet((sender_cc_t*)sender, packet_id, retrans, size);
	else
		return -1;
}

static int razor_bbr_sender_add_packet(razor_sender_t* sender, uint32_t packet_id, int retrans, size_t size)
{
	if (sender != NULL)
		return bbr_sender_add_pace_packet((bbr_sender_t*)sender, packet_id, retrans, size);
	else
		return -1;
}

static void razor_sender_on_send(razor_sender_t* sender, uint16_t transport_seq, size_t size)
{
	if (sender != NULL)
		sender_on_send_packet((sender_cc_t*)sender, transport_seq, size);
}

static void razor_bbr_sender_on_send(razor_sender_t* sender, uint16_t transport_seq, size_t size)
{
	if (sender != NULL)
		bbr_sender_send_packet((bbr_sender_t*)sender, transport_seq, size);
}

static void razor_sender_on_feedback(razor_sender_t* sender, uint8_t* feedback, int feedback_size)
{
	if (sender != NULL)
		sender_on_feedback((sender_cc_t*)sender, feedback, feedback_size);
}

static void razor_bbr_sender_on_feedback(razor_sender_t* sender, uint8_t* feedback, int feedback_size)
{
	if (sender != NULL)
		bbr_sender_on_feedback((bbr_sender_t*)sender, feedback, feedback_size);
}

static void razor_sender_update_rtt(razor_sender_t* sender, int32_t rtt)
{
	if (sender != NULL)
		sender_cc_update_rtt((sender_cc_t*)sender, rtt);
}

static void razor_bbr_sender_update_rtt(razor_sender_t* sender, int32_t rtt)
{
	if (sender != NULL)
		bbr_sender_update_rtt((bbr_sender_t*)sender, rtt);
}

static int razor_sender_get_pacer_queue_ms(razor_sender_t* sender)
{
	if (sender != NULL)
		return (int)sender_cc_get_pacer_queue_ms((sender_cc_t*)sender);
	else
		return -1;
}

static int razor_bbr_sender_get_pacer_queue_ms(razor_sender_t* sender)
{
	if (sender != NULL)
		return (int)bbr_sender_get_pacer_queue_ms((bbr_sender_t*)sender);
	else
		return -1;
}

static int64_t razor_sender_get_first_ts(razor_sender_t* sender)
{
	if (sender != NULL)
		return sender_cc_get_first_packet_ts((sender_cc_t*)sender);
	else
		return -1;
}

static int64_t razor_bbr_sender_get_first_ts(razor_sender_t* sender)
{
	if (sender != NULL)
		return bbr_sender_get_first_packet_ts((bbr_sender_t*)sender);
	else
		return -1;
}

razor_sender_t* razor_sender_create(int type, int padding, void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms)
{
	sender_cc_t* cc;
	bbr_sender_t* bbr;
	if (type == gcc_congestion){
		cc = sender_cc_create(trigger, bitrate_cb, handler, send_cb, queue_ms);
		cc->sender.heartbeat = razor_sender_heartbeat;
		cc->sender.add_packet = razor_sender_add_packet;
		cc->sender.on_send = razor_sender_on_send;
		cc->sender.set_bitrates = razor_sender_set_bitrates;
		cc->sender.update_rtt = razor_sender_update_rtt;
		cc->sender.get_pacer_queue_ms = razor_sender_get_pacer_queue_ms;
		cc->sender.on_feedback = razor_sender_on_feedback;
		cc->sender.get_first_timestamp = razor_sender_get_first_ts;
		cc->sender.type = type;
		cc->sender.padding = padding;

		return (razor_sender_t*)cc;
	}
	else if (type == bbr_congestion){
		bbr = bbr_sender_create(trigger, bitrate_cb, handler, send_cb, queue_ms, padding);
		bbr->sender.heartbeat = razor_bbr_sender_heartbeat;
		bbr->sender.add_packet = razor_bbr_sender_add_packet;
		bbr->sender.on_send = razor_bbr_sender_on_send;
		bbr->sender.set_bitrates = razor_bbr_sender_set_bitrates;
		bbr->sender.update_rtt = razor_bbr_sender_update_rtt;
		bbr->sender.get_pacer_queue_ms = razor_bbr_sender_get_pacer_queue_ms;
		bbr->sender.on_feedback = razor_bbr_sender_on_feedback;
		bbr->sender.get_first_timestamp = razor_bbr_sender_get_first_ts;
		bbr->sender.type = type;
		bbr->sender.padding = padding;

		return (razor_sender_t*)bbr;
	}

	return NULL;

}

void razor_sender_destroy(razor_sender_t* sender)
{
	if (sender != NULL){
		if (sender->type == gcc_congestion)
			sender_cc_destroy((sender_cc_t*)sender);
		else if (sender->type == bbr_congestion)
			bbr_sender_destroy((bbr_sender_t*)sender);
	}
}
/**********************************************************************************/
static void razor_receiver_heartbeat(razor_receiver_t* receiver)
{
	if (receiver != NULL)
		receiver_cc_heartbeat((receiver_cc_t*)receiver);
}

static void razor_bbr_receiver_heartbeat(razor_receiver_t* receiver)
{
	if (receiver != NULL)
		bbr_receive_check_acked((bbr_receiver_t*)receiver);
}

static void razor_receiver_on_received(razor_receiver_t* receiver, uint16_t transport_seq, uint32_t timestamp, size_t size, int remb)
{
	if (receiver != NULL)
		receiver_cc_on_received((receiver_cc_t*)receiver, transport_seq, timestamp, size, remb);
}

static void razor_bbr_receiver_on_received(razor_receiver_t* receiver, uint16_t transport_seq, uint32_t timestamp, size_t size, int remb)
{
	if (receiver != NULL)
		bbr_receive_on_received((bbr_receiver_t*)receiver, transport_seq, timestamp, size, remb);
}

static void razor_receiver_set_min_bitrate(razor_receiver_t* receiver, uint32_t bitrate)
{
	if (receiver != NULL)
		receiver_cc_set_min_bitrate((receiver_cc_t*)receiver, bitrate);
}

static void razor_bbr_receiver_set_min_bitrate(razor_receiver_t* receiver, uint32_t bitrate)
{
	if (receiver != NULL)
		bbr_receive_set_min_bitrate((bbr_receiver_t*)receiver, bitrate);
}

static void razor_receiver_set_max_bitrate(razor_receiver_t* receiver, uint32_t bitrate)
{
	if (receiver != NULL)
		receiver_cc_set_max_bitrate((receiver_cc_t*)receiver, bitrate);
}

static void razor_bbr_receiver_set_max_bitrate(razor_receiver_t* receiver, uint32_t bitrate)
{
	if (receiver != NULL)
		bbr_receive_set_max_bitrate((bbr_receiver_t*)receiver, bitrate);
}

static void razor_receiver_cc_update_rtt(razor_receiver_t* receiver, int32_t rtt)
{
	if (receiver != NULL && rtt > 0)
		receiver_cc_update_rtt((receiver_cc_t*)receiver, rtt);
}

static void razor_bbr_receiver_update_rtt(razor_receiver_t* receiver, int32_t rtt)
{
	if (receiver != NULL && rtt > 0)
		bbr_receive_update_rtt((bbr_receiver_t*)receiver, rtt);
}

razor_receiver_t* razor_receiver_create(int type, int min_bitrate, int max_bitrate, int packet_header_size, void* handler, send_feedback_func cb)
{
	receiver_cc_t* cc;
	bbr_receiver_t* bbr;

	if (type == gcc_congestion){
		cc = receiver_cc_create(min_bitrate, max_bitrate, packet_header_size, handler, cb);
		cc->receiver.heartbeat = razor_receiver_heartbeat;
		cc->receiver.on_received = razor_receiver_on_received;
		cc->receiver.set_max_bitrate = razor_receiver_set_max_bitrate;
		cc->receiver.set_min_bitrate = razor_receiver_set_min_bitrate;
		cc->receiver.update_rtt = razor_receiver_cc_update_rtt;
		cc->receiver.type = type;

		return (razor_receiver_t *)cc;
	}
	else if (type == bbr_congestion){
		bbr = bbr_receive_create(handler, cb);
		bbr->receiver.heartbeat = razor_bbr_receiver_heartbeat;
		bbr->receiver.on_received = razor_bbr_receiver_on_received;
		bbr->receiver.set_max_bitrate = razor_bbr_receiver_set_max_bitrate;
		bbr->receiver.set_min_bitrate = razor_bbr_receiver_set_min_bitrate;
		bbr->receiver.update_rtt = razor_bbr_receiver_update_rtt;
		bbr->receiver.type = type;

		return (razor_receiver_t *)bbr;
	}

	return NULL;
}

void razor_receiver_destroy(razor_receiver_t* receiver)
{
	if (receiver != NULL){
		if (receiver->type == gcc_congestion)
			receiver_cc_destroy((receiver_cc_t*)receiver);
		else if (receiver->type == bbr_congestion)
			bbr_receive_destroy((bbr_receiver_t*)receiver);
	}
}





