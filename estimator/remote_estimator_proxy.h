/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __remote_estimator_proxy_h_
#define __remote_estimator_proxy_h_

#include "estimator_common.h"
#include "cf_skiplist.h"
#include "cf_platform.h"
#include "cf_unwrapper.h"
#include "cf_stream.h"

typedef struct
{
	size_t			header_size;				/*通信报文头的固定大小*/
	int64_t			hb_ts;						/*心跳时间戳，UNIX绝对时间，毫秒为单位*/
	int64_t			send_interval_ms;			/*发送反馈间隔时间，按照5%的总可用带宽计算间隔时间*/

	uint32_t		ssrc;

	int64_t			wnd_start_seq;			
	int64_t			max_arrival_seq;

	skiplist_t*		arrival_times;
	cf_unwrapper_t	unwrapper;

	uint32_t		feelback_sequence;

}estimator_proxy_t;

estimator_proxy_t*	estimator_proxy_create(size_t packet_size, uint32_t ssrc);
void				estimator_proxy_destroy(estimator_proxy_t* proxy);

void				estimator_proxy_incoming(estimator_proxy_t* proxy, int64_t arrival_ts, uint32_t ssrc, uint16_t seq);
int					estimator_proxy_heartbeat(estimator_proxy_t* proxy, int64_t cur_ts, feedback_msg_t* msg);
void				estimator_proxy_bitrate_changed(estimator_proxy_t* proxy, uint32_t bitrate);


#endif



