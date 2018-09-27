/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_proto.h"

static inline void sim_connect_encode(bin_stream_t* strm, sim_connect_t* body)
{
	mach_uint32_write(strm, body->cid);
	mach_data_write(strm, body->token, body->token_size);
	mach_int8_write(strm, body->cc_type);
}

static inline int sim_connect_decode(bin_stream_t* strm, sim_connect_t* body)
{
	mach_uint32_read(strm, &body->cid);
	body->token_size = mach_data_read(strm, body->token, SIM_TOKEN_SIZE);
	if (body->token_size == READ_DATA_ERROR)
		body->token_size = 0;
	mach_int8_read(strm, &body->cc_type);

	return 0;
}

static inline void sim_connect_ack_encode(bin_stream_t* strm, sim_connect_ack_t* body)
{
	mach_uint32_write(strm, body->cid);
	mach_uint32_write(strm, body->result);
}

static inline int sim_connect_ack_decode(bin_stream_t* strm, sim_connect_ack_t* body)
{
	mach_uint32_read(strm, &body->cid);
	mach_uint32_read(strm, &body->result);

	return 0;
}

static inline void sim_disconnect_encode(bin_stream_t* strm, sim_disconnect_t* body)
{
	mach_uint32_write(strm, body->cid);
}

static inline int sim_disconnect_decode(bin_stream_t* strm, sim_disconnect_t* body)
{
	mach_uint32_read(strm, &body->cid);
	return 0;
}

static inline void sim_ping_encode(bin_stream_t* strm, sim_ping_t* body)
{
	mach_int64_write(strm, body->ts);
}

static inline int sim_ping_decode(bin_stream_t* strm, sim_ping_t* body)
{
	mach_int64_read(strm, &body->ts);
	return 0;
}

static inline void sim_feedbak_encode(bin_stream_t* strm, sim_feedback_t* body)
{
	mach_uint32_write(strm, body->base_packet_id);
	mach_data_write(strm, body->feedback, body->feedback_size);
}

static inline int sim_feedbak_decode(bin_stream_t* strm, sim_feedback_t* body)
{
	mach_uint32_read(strm, &body->base_packet_id);
	body->feedback_size = mach_data_read(strm, body->feedback, SIM_FEEDBACK_SIZE);
	if (body->feedback_size == READ_DATA_ERROR)
		body->feedback_size = 0;

	return 0;
}

#define U16_MAX 65535
#define U8_MAX 255

static inline void sim_segment_encode(bin_stream_t* strm, sim_segment_t* body)
{
	uint8_t mask = (body->ftype & 0x01);
	if (body->packet_id > U16_MAX)
		mask |= (1 << 7);

	if (body->fid > U16_MAX)
		mask |= (1 << 6);

	if (body->total > U8_MAX)
		mask |= (1 << 5);

	if (body->remb == 0)
		mask |= (1 << 4);

	mach_uint8_write(strm, mask);
	mach_uint8_write(strm, body->payload_type);

	if (body->packet_id > U16_MAX)
		mach_uint32_write(strm, body->packet_id);
	else
		mach_uint16_write(strm, body->packet_id);

	if (body->fid > U16_MAX)
		mach_uint32_write(strm, body->fid);
	else
		mach_uint16_write(strm, body->fid);

	mach_uint32_write(strm, body->timestamp);

	if (body->total > U8_MAX){
		mach_uint16_write(strm, body->index);
		mach_uint16_write(strm, body->total);
	}
	else{
		mach_uint8_write(strm, (uint8_t)body->index);
		mach_uint8_write(strm, (uint8_t)body->total);
	}

	mach_uint16_write(strm, body->send_ts);
	mach_uint16_write(strm, body->transport_seq);

	mach_data_write(strm, body->data, body->data_size);
}

static inline int sim_segment_decode(bin_stream_t* strm, sim_segment_t* body)
{
	uint8_t mask = 0;
	uint8_t v8;
	uint16_t v16;

	mach_uint8_read(strm, &mask);
	mach_uint8_read(strm, &body->payload_type);

	body->ftype = mask & 0x01;
	if (mask & (1 << 7))
		mach_uint32_read(strm, &body->packet_id);
	else{
		mach_uint16_read(strm, &v16);
		body->packet_id = v16;
	}

	if (mask & (1 << 6))
		mach_uint32_read(strm, &body->fid);
	else{
		mach_uint16_read(strm, &v16);
		body->fid = v16;
	}

	mach_uint32_read(strm, &body->timestamp);
	if (mask & (1 << 5)){
		mach_uint16_read(strm, &body->index);
		mach_uint16_read(strm, &body->total);
	}
	else{
		mach_uint8_read(strm, &v8);
		body->index = v8;
		mach_uint8_read(strm, &v8);
		body->total = v8;
	}

	if (mask & (1 << 4))
		body->remb = 0;
	else
		body->remb = 0xff;

	mach_uint16_read(strm, &body->send_ts);
	mach_uint16_read(strm, &body->transport_seq);

	body->data_size = mach_data_read(strm, body->data, SIM_VIDEO_SIZE);
	if (body->data_size == READ_DATA_ERROR)
		body->data_size = 0;

	return 0;
}

static inline void sim_segment_ack_encode(bin_stream_t* strm, sim_segment_ack_t* body)
{
	int i;
	mach_uint32_write(strm, body->base_packet_id);
	mach_uint32_write(strm, body->acked_packet_id);
	
	mach_uint8_write(strm, body->nack_num);
	for (i = 0; i < body->nack_num; ++i)
		mach_uint16_write(strm, body->nack[i]);
}

static inline int sim_segment_ack_decode(bin_stream_t* strm, sim_segment_ack_t* body)
{
	int i;

	mach_uint32_read(strm, &body->base_packet_id);
	mach_uint32_read(strm, &body->acked_packet_id);
	mach_uint8_read(strm, &body->nack_num);
	body->nack_num = SU_MIN(body->nack_num, NACK_NUM);
	for (i = 0; i < body->nack_num; ++i)
		mach_uint16_read(strm, &body->nack[i]);
	return 0;
}

static inline void sim_fir_encode(bin_stream_t* strm, sim_fir_t* body)
{
	mach_uint32_write(strm, body->fir_seq);
}

static inline int sim_fir_decode(bin_stream_t* strm, sim_fir_t* body)
{
	mach_uint32_read(strm, &body->fir_seq);
	return 0;
}

static inline void sim_pad_encode(bin_stream_t* strm, sim_pad_t* body)
{
	mach_uint32_write(strm, body->send_ts);
	mach_uint16_write(strm, body->transport_seq);

	mach_data_write(strm, body->data, body->data_size);
}

static inline int sim_pad_decode(bin_stream_t* strm, sim_pad_t* body)
{
	mach_uint32_read(strm, &body->send_ts);
	mach_uint16_read(strm, &body->transport_seq);

	body->data_size = mach_data_read(strm, body->data, PADDING_DATA_SIZE);
	if (body->data_size == READ_DATA_ERROR)
		body->data_size = 0;

	return 0;
}












