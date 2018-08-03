/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <stdlib.h>
#include "feedback_adapter.h"
#include "razor_log.h"

#define k_history_cache_ms		60000
void feedback_adapter_init(feedback_adapter_t* adapter)
{ 
	int i;
	for (i = 0; i < FEEDBACK_RTT_WIN_SIZE; ++i){
		adapter->rtts[i] = -1;
	}
	 
	adapter->min_feedback_rtt = 10;
	adapter->num = 0;
	adapter->index = 0;

	adapter->hist = sender_history_create(k_history_cache_ms);
}
 
void feedback_adapter_destroy(feedback_adapter_t* adapter)
{
	if (adapter != NULL && adapter->hist != NULL){
		sender_history_destroy(adapter->hist);
		adapter->hist = NULL;
	}
}

void feedback_add_packet(feedback_adapter_t* adapter, uint16_t seq, size_t size)
{
	packet_feedback_t packet;
	
	packet.arrival_ts = -1;
	packet.create_ts = packet.send_ts = GET_SYS_MS();
	packet.payload_size = size;
	packet.sequence_number = seq;

	sender_history_add(adapter->hist, &packet);
}

static int feedback_packet_comp(const void* arg1, const void* arg2)
{
	packet_feedback_t *p1, *p2;
	p1 = (packet_feedback_t*)arg1; 
	p2 = (packet_feedback_t*)arg2;

	if (p1->arrival_ts < p2->arrival_ts)
		return -1;
	else if (p1->arrival_ts > p2->arrival_ts)
		return 1;
	else if (p1->send_ts < p2->send_ts)
		return -1;
	else 
		return 1;
}

static inline void feedback_qsort(feedback_adapter_t* adapter)
{
	qsort(adapter->packets, adapter->num, sizeof(packet_feedback_t), feedback_packet_comp);
}

int feedback_on_feedback(feedback_adapter_t* adapter, feedback_msg_t* msg)
{
	int32_t i, feedback_rtt;

	int64_t now_ts;

	now_ts = GET_SYS_MS();
	feedback_rtt = -1;
	 
	adapter->num = 0;
	for (i = 0; i < msg->samples_num; i++){
		/*根据反馈的SEQ获取对应的报文发送信息，计算反馈RTT,更新报文到达时刻*/
		if (sender_history_get(adapter->hist, msg->samples[i].seq, &adapter->packets[adapter->num], 1) == 0){
			/*计算反馈RTT*/
			if (adapter->packets[adapter->num].send_ts > 0){
				feedback_rtt = SU_MAX(now_ts - adapter->packets[adapter->num].send_ts, feedback_rtt);
				adapter->rtts[adapter->index++ % FEEDBACK_RTT_WIN_SIZE] = feedback_rtt;
			}

			/*更新到达的值*/
			adapter->packets[adapter->num].arrival_ts = msg->samples[i].ts;
			adapter->num++;
		}
	}
	
	/*更新报文与反馈的rtt最小值*/
	if (feedback_rtt > 0){
		adapter->min_feedback_rtt = adapter->rtts[0];
		for (i = 1; i < FEEDBACK_RTT_WIN_SIZE; i++){
			if (adapter->min_feedback_rtt > adapter->rtts[i] && adapter->rtts[i] > 0)
				adapter->min_feedback_rtt = adapter->rtts[i];
		}
	}

	/*进行按到达时间的先后顺序进行排序*/
	feedback_qsort(adapter);

	return adapter->num;
}







