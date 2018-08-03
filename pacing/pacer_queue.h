/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __pacer_queue_h_
#define __pacer_queue_h_

#include "cf_platform.h"
#include "cf_skiplist.h"
#include "cf_list.h"

#define k_max_pace_queue_ms			250			/*pacer queue缓冲的最大延迟*/

typedef struct
{
	uint32_t		seq;			/*通信中的绝对seq，相当于帧的先后顺序，webRTC这块做的非常复杂，做了优先等级，对于视频来说，绝对的SEQ大小表明了先后关系，没有必要做各种等级区分*/
	int				retrans;		/*是否是重传*/
	size_t			size;			/*报文大小*/
	int64_t			que_ts;			/*放入pacer queue的时间戳*/
	int				sent;			/*是否已经发送*/
}packet_event_t;

typedef struct
{
	uint32_t		max_que_ms;		/*pacer可以接受最大的延迟*/
	size_t			total_size;
	int64_t			oldest_ts;		/*最早帧的时间戳*/
	skiplist_t*		cache;			/*按绝对SEQ排队的队列*/
	base_list_t*	l;				/*按时间先后的队列*/
}pacer_queue_t;

void					pacer_queue_init(pacer_queue_t* que, uint32_t que_ms);
void					pacer_queue_destroy(pacer_queue_t* que);

int						pacer_queue_push(pacer_queue_t* que, packet_event_t* ev);
/*获取que中最小seq的包，按顺序发出，这样防止出现大范围的抖动*/
packet_event_t*			pacer_queue_front(pacer_queue_t* que);
void					pacer_queue_sent(pacer_queue_t* que, packet_event_t* ev);

int						pacer_queue_empty(pacer_queue_t* que);
size_t					pacer_queue_bytes(pacer_queue_t* que);
int64_t					pacer_queue_oldest(pacer_queue_t* que);
/*计算que需要的码率*/
uint32_t				pacer_queue_target_bitrate_kbps(pacer_queue_t* que, int64_t now_ts);

#endif


