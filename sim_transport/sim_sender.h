/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

/*一个nack请求包的带宽限制器*/
typedef struct
{
	uint32_t*		buckets;
	uint32_t		index;
	int64_t			oldest_ts;

	int				wnd_size;			/*统计的时间窗*/
	uint32_t		wnd_bytes;
	uint32_t		threshold;			/*最大可以发送的字节数*/
}sim_sender_limiter_t;

struct __sim_sender
{
	int							actived;
	uint32_t					base_packet_id;			/*接受端报告已经接收到连续的最大报文ID*/

	uint32_t					packet_id_seed;			/*视频报文ID*/
	uint32_t					send_id_seed;			/*与CC模块关联的发送ID*/
	uint32_t					frame_id_seed;
	uint32_t					transport_seq_seed;

	int64_t						first_ts;			/*第一帧起始时间戳*/

	skiplist_t*					segs_cache;			/*视频分片cache*/
	skiplist_t*					fecs_cache;			/*fec发送窗口*/
	skiplist_t*					ack_cache;			/*ack重传分片cache*/

	razor_sender_t*				cc;					/*拥塞控制对象*/

	sim_session_t*				s;

	flex_fec_sender_t*			flex;				/*flex FEC对象*/
	base_list_t*				out_fecs;

	size_t						splits_size;
	uint16_t*					splits;
};




