/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

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

};



