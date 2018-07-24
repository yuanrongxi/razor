/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __feedback_adapter_h_
#define __feedback_adapter_h_

#include "sender_history.h"

#define FEEDBACK_RTT_WIN_SIZE 32
typedef struct
{
	sender_history_t*	hist;
	int32_t				min_feedback_rtt;
	int					index;
	int32_t				rtts[FEEDBACK_RTT_WIN_SIZE];

	int					num;
	packet_feedback_t	packets[MAX_FEELBACK_COUNT];
}feedback_adapter_t;

void				feedback_adapter_init(feedback_adapter_t* adapter);
void				feedback_adapter_destroy(feedback_adapter_t* adapter);

/*添加一个网络发送报文的记录*/
void				feedback_add_packet(feedback_adapter_t* adapter, uint16_t seq, size_t size);

/*解码网络来的feedback，并解析成packet_feedback结构序列，这个数据是remote estimator proxy反馈过来的*/
int					feedback_on_feedback(feedback_adapter_t* adapter, feedback_msg_t* msg);


#endif



