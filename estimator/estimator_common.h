/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __estimator_common_h_
#define __estimator_common_h_
#include <stdint.h>
#include "cf_stream.h"

enum {
	kBwNormal = 0,
	kBwUnderusing = 1,
	kBwOverusing = 2,
};

enum {
	kRcHold, 
	kRcIncrease, 
	kRcDecrease
};

enum { 
	kRcNearMax, 
	kRcAboveMax, 
	kRcMaxUnknown 
};

typedef struct
{
	int			state;
	uint32_t	incoming_bitrate;
	double		noise_var;
}rate_control_input_t;

#define DEFAULT_RTT 200


typedef struct
{
	int64_t			create_ts;			/*创建时间戳*/
	int64_t			arrival_ts;			/*到达时间戳*/
	int64_t			send_ts;			/*发送时间戳*/

	uint16_t		sequence_number;	/*发送通道的报文序号*/
	size_t			payload_size;		/*包数据大小*/
}packet_feedback_t;

#define init_packet_feedback(p)	\
	do{							\
		(p).create_ts = -1;		\
		(p).arrival_ts = -1;		\
		(p).send_ts = -1;		\
		(p).sequence_number = 0;\
		(p).payload_size = 0;	\
		} while (0)


typedef struct
{
	uint16_t		seq;
	int64_t			ts;
}feedback_sample_t;

enum
{
	remb_msg = 0x01,
	loss_info_msg = 0x02,
	proxy_ts_msg = 0x04,
};

#define MAX_FEELBACK_COUNT 128
typedef struct
{
	uint8_t					flag;
	/*remb_msg*/
	uint32_t				remb;
	/*loss info msg*/
	uint8_t					fraction_loss;
	int						packet_num;
	/*proxy_ts_msg*/
	int64_t					base_seq;
	int64_t					min_ts;
	uint8_t				samples_num;
	feedback_sample_t	samples[MAX_FEELBACK_COUNT];
}feedback_msg_t;

void	feedback_msg_encode(bin_stream_t* strm, feedback_msg_t* msg);
void	feedback_msg_decode(bin_stream_t* strm, feedback_msg_t* msg);

#endif


