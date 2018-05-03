/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

/*一个nack请求包的带宽限制器*/
typedef struct
{
	rate_stat_t		rate;
	int				wnd_size;			/*统计的时间窗*/
	uint32_t		max_rate_bps;		/*最大可以承受的码率*/
}sim_sender_limiter_t;

struct __sim_sender
{
	int							actived;
	uint32_t					base_packet_id;			/*接受端报告已经接收到连续的最大报文ID*/

	uint32_t					packet_id_seed;
	uint32_t					frame_id_seed;
	uint32_t					transport_seq_seed;

	int64_t						first_ts;			/*第一帧起始时间戳*/

	skiplist_t*					cache;				/*发送窗口*/

	razor_sender_t*				cc;					/*拥塞控制对象*/

	sim_session_t*				s;

	sim_sender_limiter_t		limiter;
};




