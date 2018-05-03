/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_internal.h"

#define MAX_LIMITER_RATE			(1024 * 1024 * 10)
void sim_limiter_init(sim_sender_limiter_t* limiter, int windows_size_ms)
{
	rate_stat_init(&limiter->rate, windows_size_ms, 8000);

	limiter->max_rate_bps = MAX_LIMITER_RATE;
	limiter->wnd_size = windows_size_ms;
}

void sim_limiter_destroy(sim_sender_limiter_t* limiter)
{
	rate_stat_destroy(&limiter->rate);
}

void sim_limiter_set_max_bitrate(sim_sender_limiter_t* limiter, uint32_t bitrate)
{
	limiter->max_rate_bps = bitrate;
}

/*判断是否可以进行发送报文*/
int	sim_limiter_try(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts)
{
	int ret, cur_rate;
	size_t add_bitrate;

	ret = 0;
	cur_rate = rate_stat_rate(&limiter->rate, now_ts);
	if (cur_rate > 0){
		add_bitrate = size * 8 * 1000 / limiter->wnd_size;

		if (add_bitrate + cur_rate > limiter->max_rate_bps)
			ret = -1;
	}
	return ret;
}

void sim_limiter_update(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts)
{
	rate_stat_update(&limiter->rate, size, now_ts);
}

