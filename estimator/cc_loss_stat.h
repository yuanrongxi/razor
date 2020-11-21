/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __cc_loss_stat_h_
#define __cc_loss_stat_h_

#include <stdint.h>
#include "cf_platform.h"
#include "cf_skiplist.h"
#include "cf_unwrapper.h"

/*接收端统计丢包，没有采用webRTC中比较复杂的方式，而是采用了一种简单近似的方式来实现*/
typedef struct
{
	int64_t			stat_ts;
	int64_t			max_id;				/*接收到最大序号*/
	skiplist_t*		list;

	cf_unwrapper_t	wrapper;			/*id转换器*/
}cc_loss_statistics_t;

void			loss_statistics_init(cc_loss_statistics_t* loss_stat);
void			loss_statistics_destroy(cc_loss_statistics_t* loss_stat);

int				loss_statistics_calculate(cc_loss_statistics_t* loss_stat, int64_t now_ts, uint8_t* fraction_loss, int* num);
void			loss_statistics_incoming(cc_loss_statistics_t* loss_stat, uint16_t seq, int64_t now_ts);

#endif



