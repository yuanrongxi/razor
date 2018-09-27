/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "cf_platform.h"
#include "sim_proto.inl"
#include "cf_crc32.h"

#define CRC_VAL	0x0e3dfc0a

static void sim_encode_header(bin_stream_t* strm, sim_header_t* header)
{
	mach_uint8_write(strm, header->ver);
	mach_uint8_write(strm, header->mid);
	mach_uint32_write(strm, header->uid);
}

int sim_decode_header(bin_stream_t* strm, sim_header_t* header)
{
	uint32_t src, crc;
	uint8_t* pos = strm->data + strm->used - sizeof(uint32_t);
	src = mach_get_4(pos);

	/*校验CRC*/
	crc = crc32(CRC_VAL, strm->data, strm->used - sizeof(uint32_t));
	if (src == crc){
		mach_uint8_read(strm, &header->ver);
		mach_uint8_read(strm, &header->mid);
		mach_uint32_read(strm, &header->uid);

		return 0;
	}
	else
		return -1;
}

void sim_encode_msg(bin_stream_t* strm, sim_header_t* header, void* body)
{
	uint32_t crc = 0;

	bin_stream_rewind(strm, 1);

	sim_encode_header(strm, header);
	switch (header->mid){
	case SIM_CONNECT:
		sim_connect_encode(strm, body);
		break;

	case SIM_CONNECT_ACK:
	case SIM_DISCONNECT_ACK:
		sim_connect_ack_encode(strm, body);
		break;

	case SIM_DISCONNECT:
		sim_disconnect_encode(strm, body);
		break;

	case SIM_PING:
	case SIM_PONG:
		sim_ping_encode(strm, body);
		break;

	case SIM_SEG:
		sim_segment_encode(strm, body);
		break;

	case SIM_SEG_ACK:
		sim_segment_ack_encode(strm, body);
		break;

	case SIM_FEEDBACK:
		sim_feedbak_encode(strm, body);
		break;

	case SIM_FIR:
		sim_fir_encode(strm, body);
		break;

	case SIM_PAD:
		sim_pad_encode(strm, body);
		break;

	default:
		;
	}

	/*增加CRC值*/
	crc = crc32(CRC_VAL, strm->data, strm->used);
	mach_uint32_write(strm, crc);
}

int sim_decode_msg(bin_stream_t* strm, sim_header_t* header, void* body)
{
	int ret = -1;
	switch (header->mid){
	case SIM_CONNECT:
		ret = sim_connect_decode(strm, body);
		break;

	case SIM_CONNECT_ACK:
	case SIM_DISCONNECT_ACK:
		ret = sim_connect_ack_decode(strm, body);
		break;

	case SIM_DISCONNECT:
		ret = sim_disconnect_decode(strm, body);
		break;

	case SIM_PING:
	case SIM_PONG:
		ret = sim_ping_decode(strm, body);
		break;

	case SIM_SEG:
		ret = sim_segment_decode(strm, body);
		break;

	case SIM_SEG_ACK:
		ret = sim_segment_ack_decode(strm, body);
		break;

	case SIM_FEEDBACK:
		ret = sim_feedbak_decode(strm, body);
		break;

	case SIM_FIR:
		ret = sim_fir_decode(strm, body);
		break;

	case SIM_PAD:
		ret = sim_pad_decode(strm, body);
		break;

	default:
		;
	}

	return ret;
}

const char* sim_get_msg_name(uint8_t msg_id)
{
	switch (msg_id){
	case SIM_CONNECT:
		return "SIM_CONNECT";

	case SIM_CONNECT_ACK:
		return "SIM_CONNECT_ACK";

	case SIM_DISCONNECT:
		return "SIM_DISCONNECT";

	case SIM_DISCONNECT_ACK:
		return "SIM_DISCONNECT_ACK";

	case SIM_PING:
		return "SIM_PING";
		
	case SIM_PONG:
		return "SIM_PONG";

	case SIM_SEG:
		return "SIM_SEG";

	case SIM_SEG_ACK:
		return "SIM_SEG_ACK";

	case SIM_FEEDBACK:
		return "SIM_FEEDBACK";

	case SIM_FIR:
		return "SIM_FIR";

	case SIM_PAD:
		return "SIM_PAD";

	default:
		return "unknown message";
	}
}


