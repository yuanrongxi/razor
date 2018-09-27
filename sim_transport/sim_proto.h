/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sim_proto_h_001__
#define __sim_proto_h_001__

#include "cf_platform.h"
#include "cf_stream.h"

/*这是模拟传输的协议定义，这里只是简单的点对点通信模拟，不涉及路由优化、连接安全性，不建议生产环境使用*/

enum
{
	MIN_MSG_ID	=	0x10,

	SIM_CONNECT,			/*连接接收端请求*/
	SIM_CONNECT_ACK,		/*响应*/

	SIM_DISCONNECT,
	SIM_DISCONNECT_ACK,

	SIM_PING,
	SIM_PONG,

	SIM_SEG,
	SIM_SEG_ACK,
	SIM_FEEDBACK,
	SIM_FIR,				/*请求关键帧重传*/
	SIM_PAD,				/*padding报文*/

	MAX_MSG_ID
};

#define protocol_ver			0x01

typedef struct
{
	uint8_t		ver;			/*协议版本，默认为0x01*/
	uint8_t		mid;			/*消息ID*/
	uint32_t	uid;			/*消息发起方ID*/
}sim_header_t;

#define INIT_SIM_HEADER(h, msg_id, userid) \
	h.ver = protocol_ver;	\
	h.mid = (msg_id);		\
	h.uid = (userid)

#define SIM_HEADER_SIZE			6
#define SIM_VIDEO_SIZE			1000
#define SIM_TOKEN_SIZE			128
#define NACK_NUM				80
#define SIM_FEEDBACK_SIZE		1000
typedef struct
{
	uint32_t cid;						/*协商的呼叫ID，每次是一个随机值,和disconnect等消息保持一致*/
	uint16_t token_size;				/*token是一个验证信息，可以用类似证书的方式来进行验证*/
	uint8_t	 token[SIM_TOKEN_SIZE];
	uint8_t	 cc_type;					/*拥塞控制类型*/
}sim_connect_t;

typedef struct
{
	uint32_t cid;
	uint32_t result;
}sim_connect_ack_t;

typedef struct
{
	uint32_t cid;
}sim_disconnect_t;

typedef sim_connect_ack_t sim_disconnect_ack_t;

typedef struct
{
	uint32_t	packet_id;				/*包序号*/
	uint32_t	fid;					/*帧序号*/
	uint32_t	timestamp;				/*帧时间戳*/
	uint16_t	index;					/*帧分包序号*/
	uint16_t	total;					/*帧分包总数*/
	uint8_t		ftype;					/*视频帧类型*/
	uint8_t		payload_type;			/*编码器类型*/

	uint8_t		remb;					/*0表示开启remb, 其他表示不开启*/
	uint16_t	send_ts;				/*发送时刻相对帧产生时刻的时间戳*/
	uint16_t	transport_seq;			/*传输通道序号，这个是传输通道每发送一个报文，它就自增长1，而且重发报文也会增长*/
	
	uint16_t	data_size;
	uint8_t		data[SIM_VIDEO_SIZE];
}sim_segment_t;

#define SIM_SEGMENT_HEADER_SIZE (SIM_HEADER_SIZE + 24)

typedef struct
{
	uint32_t	base_packet_id;			/*被接收端确认连续最大的包ID*/
	uint32_t	acked_packet_id;		/*立即确认的报文序号id,用于计算rtt*/
	/*重传的序列*/
	uint8_t		nack_num;
	uint16_t	nack[NACK_NUM];
}sim_segment_ack_t;

typedef struct
{
	int64_t	ts;							/*心跳时刻*/
}sim_ping_t;

typedef sim_ping_t sim_pong_t;

typedef struct
{
	uint32_t	base_packet_id;						/*被接收端确认连续最大的包ID*/

	uint16_t	feedback_size;						/*cc中的反馈信息,由接收端的拥塞控制对象产生*/
	uint8_t		feedback[SIM_FEEDBACK_SIZE];
}sim_feedback_t;

typedef struct
{
	uint32_t	fir_seq;							/*fir的序号，每次接收端触发条件时递增*/
}sim_fir_t;

#define PADDING_DATA_SIZE  500
typedef struct
{
	uint32_t	send_ts;				/*发送时刻相对帧产生时刻的时间戳*/
	uint16_t	transport_seq;			/*传输通道序号，这个是传输通道每发送一个报文，它就自增长1，而且重发报文也会增长*/

	uint16_t	data_size;
	uint8_t		data[PADDING_DATA_SIZE];
}sim_pad_t;

void							sim_encode_msg(bin_stream_t* strm, sim_header_t* header, void* body);
int								sim_decode_header(bin_stream_t* strm, sim_header_t* header);
int								sim_decode_msg(bin_stream_t* strm, sim_header_t* header, void* body);
const char*						sim_get_msg_name(uint8_t msg_id);

#endif



