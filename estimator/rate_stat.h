/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __rate_stat_h_
#define __rate_stat_h_

#include "cf_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int			sum;
	int			sample;
}rate_bucket_t;

/*单位时间带宽统计器*/
typedef struct
{
	int64_t			oldest_ts;
	int				oldest_index;

	rate_bucket_t*	buckets;
	int				wnd_size;

	float			scale;

	int				accumulated_count;
	int				sample_num;
}rate_stat_t;

void				rate_stat_init(rate_stat_t* rate, int wnd_size, float scale);
void				rate_stat_destroy(rate_stat_t* rate);

void				rate_stat_reset(rate_stat_t* rate);

void				rate_stat_update(rate_stat_t* rate, size_t count, int64_t now_ts);

int					rate_stat_rate(rate_stat_t* rate, int64_t now_ts);

#endif

#ifdef __cplusplus
}
#endif 


