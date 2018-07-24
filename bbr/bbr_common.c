#include "bbr_common.h"

void bbr_feedback_msg_encode(bin_stream_t* strm, bbr_feedback_msg_t* msg)
{
	int i;

	bin_stream_rewind(strm, 1);
	mach_uint8_write(strm, msg->flag);

	if ((msg->flag & bbr_loss_info_msg) == bbr_loss_info_msg){
		mach_uint8_write(strm, msg->fraction_loss);
		mach_int32_write(strm, msg->packet_num);
	}

	mach_int64_write(strm, msg->base_seq);
	mach_uint8_write(strm, msg->samples_num);
	for (i = 0; i < msg->samples_num; ++i){
		mach_uint16_write(strm, msg->samples[i]);
	}
}

void bbr_feedback_msg_decode(bin_stream_t* strm, bbr_feedback_msg_t* msg)
{
	int i;
	
	msg->samples_num = 0;

	mach_uint8_read(strm, &msg->flag);

	if ((msg->flag & bbr_loss_info_msg) == bbr_loss_info_msg){
		mach_uint8_read(strm, &msg->fraction_loss);
		mach_int32_read(strm, &msg->packet_num);
	}

	mach_int64_read(strm, &msg->base_seq);

	mach_uint8_read(strm, &msg->samples_num);
	if (msg->samples_num > MAX_BBR_FEELBACK_COUNT)
		msg->samples_num = 0;

	for (i = 0; i < msg->samples_num; ++i)
		mach_uint16_read(strm, &msg->samples[i]);
}
