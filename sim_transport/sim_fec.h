/*-
* Copyright (c) 2017-2019 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

struct __sim_receiver_fec
{
	uint32_t			base_id;
	uint32_t			max_ts;

	int64_t				evict_ts;

	skiplist_t*			segs_cache;
	skiplist_t*			flexes;
	skiplist_t*			recover_packets;
	base_list_t*		out;
};
