#include "bbr_common.h"

void bbr_feedback_msg_encode(bin_stream_t* strm, bbr_feedback_msg_t* msg)
{
	bin_stream_rewind(strm, 1);
	mach_uint8_write(strm, msg->flag);

	if ((msg->flag & bbr_loss_info_msg) == bbr_loss_info_msg){
		mach_uint8_write(strm, msg->fraction_loss);
		mach_int32_write(strm, msg->packet_num);
	}

	mach_uint16_write(strm, msg->sample);
}

void bbr_feedback_msg_decode(bin_stream_t* strm, bbr_feedback_msg_t* msg)
{
	mach_uint8_read(strm, &msg->flag);

	if ((msg->flag & bbr_loss_info_msg) == bbr_loss_info_msg){
		mach_uint8_read(strm, &msg->fraction_loss);
		mach_int32_read(strm, &msg->packet_num);
	}

	mach_uint16_read(strm, &msg->sample);
}
