#include "bbr_receiver.h"
#include "bbr_common.h"

#define k_bbr_recv_wnd_size 4096
#define k_bbr_feedback_time 100


bbr_receiver_t* bbr_receive_create(void* handler, send_feedback_func cb)
{
	bbr_receiver_t* cc = calloc(1, sizeof(bbr_receiver_t));
	cc->handler = handler;
	cc->send_cb = cb;

	cc->base_seq = -1;

	cc->feedback_ts = -1;

	init_unwrapper16(&cc->unwrapper);
	loss_statistics_init(&cc->loss_stat);

	bin_stream_init(&cc->strm);

	return cc;
}

void bbr_receive_destroy(bbr_receiver_t* cc)
{
	if (cc == NULL)
		return;

	bin_stream_destroy(&cc->strm);
	loss_statistics_destroy(&cc->loss_stat);
	free(cc);
}

void bbr_receive_check_acked(bbr_receiver_t* cc)
{

}

#define BBR_BACK_WINDOW 500
void bbr_receive_on_received(bbr_receiver_t* cc, uint16_t seq, uint32_t timestamp, size_t size, int remb)
{
	bbr_feedback_msg_t msg;
	int64_t sequence, now_ts;

	/*统计丢包*/
	loss_statistics_incoming(&cc->loss_stat, seq);

	sequence = wrap_uint16(&cc->unwrapper, seq);
	if (sequence > cc->base_seq + 32767)
		return;

	now_ts = GET_SYS_MS();

	cc->base_seq = SU_MAX(cc->base_seq, sequence);

	/*判断proxy estimator是否可以发送报告*/
	msg.flag |= bbr_acked_msg;
	msg.sample = seq;

	/*判断丢包消息*/
	if (loss_statistics_calculate(&cc->loss_stat, now_ts, &msg.fraction_loss, &msg.packet_num) == 0)
		msg.flag |= bbr_loss_info_msg;

	bbr_feedback_msg_encode(&cc->strm, &msg);
	cc->send_cb(cc->handler, cc->strm.data, cc->strm.used);
}

void bbr_receive_update_rtt(bbr_receiver_t* cc, int32_t rtt)
{

}

void bbr_receive_set_min_bitrate(bbr_receiver_t* cc, int min_bitrate)
{

}

void bbr_receive_set_max_bitrate(bbr_receiver_t* cc, int max_bitrate)
{

}

