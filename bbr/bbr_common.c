/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
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

	mach_uint8_write(strm, msg->sampler_num);
	for (i = 0; i < msg->sampler_num; ++i){
		mach_uint16_write(strm, msg->samplers[i].seq);
		mach_uint16_write(strm, msg->samplers[i].delta_ts);
	}
}

void bbr_feedback_msg_decode(bin_stream_t* strm, bbr_feedback_msg_t* msg)
{
	int i;

	mach_uint8_read(strm, &msg->flag);

	if ((msg->flag & bbr_loss_info_msg) == bbr_loss_info_msg){
		mach_uint8_read(strm, &msg->fraction_loss);
		mach_int32_read(strm, &msg->packet_num);
	}

	mach_uint8_read(strm, &msg->sampler_num);
	if (msg->sampler_num > MAX_BBR_FEELBACK_COUNT)
		msg->sampler_num = 0;

	for (i = 0; i < msg->sampler_num; ++i){
		mach_uint16_read(strm, &msg->samplers[i].seq);
		mach_uint16_read(strm, &msg->samplers[i].delta_ts);
	}
}
