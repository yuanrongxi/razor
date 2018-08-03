/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_internal.h"

#define MAX_LIMITER_RATE			(1024 * 800)
void sim_limiter_init(sim_sender_limiter_t* limiter, int windows_size_ms)
{
	limiter->wnd_size = windows_size_ms;
	limiter->threshold = MAX_LIMITER_RATE * windows_size_ms / (1000 * 8);
	limiter->buckets = calloc(limiter->wnd_size, sizeof(uint32_t));
	limiter->index = 0;
	limiter->wnd_bytes = 0;
	limiter->oldest_ts = -1;
}

void sim_limiter_destroy(sim_sender_limiter_t* limiter)
{
	free(limiter->buckets);
}

void sim_limiter_resest(sim_sender_limiter_t* limiter)
{
	int i;
	for (i = 0; i < limiter->wnd_size; i++)
		limiter->buckets[i] = 0;

	limiter->index = 0;
	limiter->wnd_bytes = 0;
	limiter->oldest_ts = -1;

	limiter->threshold = MAX_LIMITER_RATE * limiter->wnd_size / (1000 * 8);
}

void sim_limiter_set_max_bitrate(sim_sender_limiter_t* limiter, uint32_t bitrate)
{
	limiter->threshold = bitrate * limiter->wnd_size / (1000 * 8);
}

static void sim_limiter_remove(sim_sender_limiter_t* limiter, int64_t now_ts)
{
	int offset, i, index;

	offset = now_ts - limiter->oldest_ts;
	if (offset < limiter->wnd_size + limiter->index){
		for (i = limiter->index + 1; i <= offset; ++i){
			index = i % limiter->wnd_size;
			if (limiter->wnd_bytes >= limiter->buckets[index])
				limiter->wnd_bytes -= limiter->buckets[index];
			else
				limiter->wnd_bytes = 0;

			limiter->buckets[index] = 0;
		}
	}
	else{
		for (i = 0; i < limiter->wnd_size; ++i){
			index = i % limiter->wnd_size;
			limiter->buckets[index] = 0;
		}

		limiter->wnd_bytes = 0;
	}

	limiter->index = offset;
}

/*判断是否可以进行发送报文*/
int	sim_limiter_try(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts)
{
	if (now_ts < limiter->oldest_ts)
		return 0;

	if (limiter->oldest_ts == -1)
		limiter->oldest_ts = now_ts;

	sim_limiter_remove(limiter, now_ts);

	return (size + limiter->wnd_bytes > limiter->threshold) ? -1 : 0;
}

void sim_limiter_update(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts)
{
	int offset;

	if (now_ts < limiter->oldest_ts)
		return;

	if (limiter->oldest_ts == -1)
		limiter->oldest_ts = now_ts;

	offset = (now_ts - limiter->oldest_ts);
	if (offset > 0)
		sim_limiter_remove(limiter, now_ts);

	/*设置当前时刻*/
	limiter->buckets[offset % limiter->wnd_size] += size;
	limiter->wnd_bytes += size;
}
