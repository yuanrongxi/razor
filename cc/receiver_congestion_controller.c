/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "receiver_congestion_controller.h"
#include "razor_log.h"

receiver_cc_t* receiver_cc_create(int min_bitrate, int max_bitrate, int packet_header_size, void* handler, send_feedback_func cb)
{
	receiver_cc_t* cc = calloc(1, sizeof(receiver_cc_t));
	cc->min_bitrate = min_bitrate;
	cc->max_bitrate = max_bitrate;
	cc->proxy = estimator_proxy_create(packet_header_size, 0);
	cc->rbe = rbe_create();

	cc->handler = handler;
	cc->send_cb = cb;

	rbe_set_min_bitrate(cc->rbe, min_bitrate);
	rbe_set_max_bitrate(cc->rbe, max_bitrate);

	loss_statistics_init(&cc->loss_stat);

	razor_info("create razor's receiver\n");

	return cc;
}

void receiver_cc_destroy(receiver_cc_t* cc)
{
	if (cc == NULL)
		return;

	if (cc->rbe != NULL){
		rbe_destroy(cc->rbe);
		cc->rbe = NULL;
	}

	if (cc->proxy != NULL){
		estimator_proxy_destroy(cc->proxy);
		cc->proxy = NULL;
	}

	loss_statistics_destroy(&cc->loss_stat);

	free(cc);

	razor_info("destroy razor's receiver\n");
}

void receiver_cc_heartbeat(receiver_cc_t* cc)
{
	int64_t now_ts;
	bin_stream_t strm;
	feedback_msg_t msg;

	bin_stream_init(&strm);

	msg.flag = 0;
	now_ts = GET_SYS_MS();

	/*对remb评估对象做心跳，如果remb做了码率更新，进行汇报给发送端*/
	if (rbe_heartbeat(cc->rbe, now_ts, &msg.remb) == 0)
		msg.flag |= remb_msg;

	/*判断丢包消息*/
	if (loss_statistics_calculate(&cc->loss_stat, now_ts, &msg.fraction_loss, &msg.packet_num) == 0)
		msg.flag |= loss_info_msg;

	/*判断proxy estimator是否可以发送报告*/
	if (estimator_proxy_heartbeat(cc->proxy, now_ts, &msg) == 0)
		msg.flag |= proxy_ts_msg;

	if (msg.flag != 0){
		feedback_msg_encode(&strm, &msg);
		cc->send_cb(cc->handler, strm.data, strm.used);
	}

	bin_stream_destroy(&strm);
}

void receiver_cc_on_received(receiver_cc_t* cc, uint16_t seq, uint32_t timestamp, size_t size, int remb)
{
	int64_t now_ts = GET_SYS_MS();
	if (remb == 0)
		rbe_incoming_packet(cc->rbe, timestamp, now_ts, size, now_ts);
	else
		estimator_proxy_incoming(cc->proxy, now_ts, 0, seq);

	/*进行丢包统计*/
	loss_statistics_incoming(&cc->loss_stat, seq);
}

void receiver_cc_update_rtt(receiver_cc_t* cc, int32_t rtt)
{
	rbe_update_rtt(cc->rbe, rtt);
	/*razor_info("razor's receiver update rtt, rtt = %dms\n", rtt);*/
}

void receiver_cc_set_min_bitrate(receiver_cc_t* cc, int min_bitrate)
{
	cc->min_bitrate = min_bitrate;
	rbe_set_min_bitrate(cc->rbe, min_bitrate);
	razor_info("receiver set min bitrate, bitrate = %ubps\n", min_bitrate);
}

void receiver_cc_set_max_bitrate(receiver_cc_t* cc, int max_bitrate)
{
	cc->max_bitrate = max_bitrate;
	rbe_set_max_bitrate(cc->rbe, max_bitrate);
	razor_info("receiver set max bitrate, bitrate = %ubps\n", max_bitrate);
}

