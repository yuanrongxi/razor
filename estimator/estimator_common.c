#include "estimator_common.h"

void feedback_msg_encode(bin_stream_t* strm, feedback_msg_t* msg)
{
	int i;
	mach_int64_write(strm, msg->base_seq);
	mach_int64_write(strm, msg->min_ts);

	mach_uint8_write(strm, msg->samples_num);
	for (i = 0; i < msg->samples_num; ++i){
		mach_uint16_write(strm, msg->samples[i].seq);
		mach_uint16_write(strm, msg->samples[i].ts - msg->min_ts);
	}
}

void feedback_msg_decode(bin_stream_t* strm, feedback_msg_t* msg)
{
	int i;
	uint16_t delta_ts;
	mach_int16_read(strm, &msg->base_seq);
	mach_int64_read(strm, &msg->min_ts);

	mach_uint8_read(strm, &msg->samples_num);
	if (msg->samples_num > MAX_FEELBACK_COUNT)
		msg->samples_num = 0;

	for (i = 0; i < msg->samples_num; ++i){
		mach_uint16_read(strm, &msg->samples[i].seq);
		mach_uint16_read(strm, &delta_ts);
		msg->samples[i].ts = msg->min_ts + delta_ts;
	}
}




