#include "cf_platform.h"
#include "sim_proto.inl"

void sim_encode_msg(bin_stream_t* strm, sim_header_t* header, void* body)
{
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

	default:
		;
	}
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

	default:
		return "unknown message";
	}
}


