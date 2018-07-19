#include "bbr_receiver.h"
#include "bbr_common.h"

#define k_bbr_recv_wnd_size 4096
#define k_bbr_feedback_time 100


bbr_receiver_t* bbr_receive_create(void* handler, send_feedback_func cb)
{
	int i;

	bbr_receiver_t* cc = calloc(1, sizeof(bbr_receiver_t));
	cc->handler = handler;
	cc->send_cb = cb;

	cc->base_seq = -1;
	cc->min_seq = -1;
	cc->max_seq = -1;

	cc->feedback_ts = -1;

	init_unwrapper16(&cc->unwrapper);
	loss_statistics_init(&cc->loss_stat);

	cc->array_size = k_bbr_recv_wnd_size;
	cc->array = malloc(sizeof(int64_t) * cc->array_size);
	for (i = 0; i < cc->array_size; ++i)
		cc->array[i] = -1;

	bin_stream_init(&cc->strm);

	return cc;
}

void bbr_receive_destroy(bbr_receiver_t* cc)
{
	if (cc == NULL)
		return;

	if (cc->array != NULL){
		free(cc->array);
		cc->array = NULL;
	}

	bin_stream_destroy(&cc->strm);
	loss_statistics_destroy(&cc->loss_stat);
	free(cc);
}

static int bbr_receive_pack_acked(bbr_receiver_t* cc, int64_t now_ts, bbr_feedback_msg_t* msg)
{
	int pos;
	int64_t i, new_base_seq = -1;

	if (cc->base_seq + 2 > cc->max_seq || cc->feedback_ts + k_bbr_feedback_time > now_ts && cc->base_seq + 32 >= cc->max_seq)
		return -1;

	msg->samples_num = 0;
	msg->base_seq = cc->base_seq;
	for (i = cc->base_seq; i <= cc->max_seq; ++i){
		pos = i % cc->array_size;
		
		if (cc->array[pos] > 0){
			msg->samples[msg->samples_num] = (i & 0xffff);
			++msg->samples_num;

			new_base_seq = i + 1;

			if (msg->samples_num >= MAX_BBR_FEELBACK_COUNT)
				break;
		}
	}
	/*进行到达时间序列编码*/
	if (msg->samples_num > 2){
		cc->feedback_ts = now_ts;
		cc->base_seq = new_base_seq;
		
		return 0;
	}
	else
		return -1;
}

void bbr_receive_check_acked(bbr_receiver_t* cc)
{
	bbr_feedback_msg_t msg;
	int64_t now_ts = GET_SYS_MS();

	/*判断proxy estimator是否可以发送报告*/
	if (bbr_receive_pack_acked(cc, now_ts, &msg) == 0){
		msg.flag |= bbr_acked_msg;

		/*判断丢包消息*/
		if (loss_statistics_calculate(&cc->loss_stat, now_ts, &msg.fraction_loss, &msg.packet_num) == 0)
			msg.flag |= bbr_loss_info_msg;

		bbr_feedback_msg_encode(&cc->strm, &msg);
		cc->send_cb(cc->handler, cc->strm.data, cc->strm.used);
	}
}

#define BBR_BACK_WINDOW 500
void bbr_receive_on_received(bbr_receiver_t* cc, uint16_t seq, uint32_t timestamp, size_t size, int remb)
{
	int pos;
	int64_t i, sequence, max_seq, now_ts;

	/*统计丢包*/
	loss_statistics_incoming(&cc->loss_stat, seq);

	sequence = wrap_uint16(&cc->unwrapper, seq);
	if (sequence > cc->base_seq + 32767 || sequence <= cc->min_seq)
		return;

	now_ts = GET_SYS_MS();

	max_seq = SU_MIN(sequence, cc->base_seq);

	/*让窗口保持不大于1/2的最大窗口大小，因为抖动一般不会超过半个最大窗口*/
	while (max_seq >= cc->min_seq + cc->array_size / 2){
		pos = cc->min_seq % cc->array_size;
		cc->array[pos] = -1;
		cc->min_seq ++;
		cc->base_seq = SU_MAX(cc->base_seq, cc->min_seq);
	}

	/*进行过期删除*/
	if (cc->max_seq <= cc->base_seq){
		for (i = cc->min_seq; i < max_seq; ++i){
			pos = i % cc->array_size;
			
			if (cc->array[pos] > 0)
				if (now_ts >= cc->array[pos] + BBR_BACK_WINDOW){
					cc->array[pos] = -1;
					cc->min_seq = i;
				}
		}
	}

	pos = sequence % cc->array_size;
	cc->array[pos] = now_ts;

	if (cc->base_seq == -1){
		cc->base_seq = seq;
		cc->min_seq = seq;
	}
	else if (cc->base_seq > sequence)
		cc->base_seq = sequence;

	cc->max_seq = SU_MAX(cc->max_seq, sequence);

	bbr_receive_check_acked(cc);
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

