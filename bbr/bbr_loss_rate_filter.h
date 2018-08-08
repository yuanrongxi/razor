/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_loss_rate_filter_h_
#define __bbr_loss_rate_filter_h_

#include "cf_platform.h"


typedef struct
{
	int			lost_count;
	int			total_count;
	double		loss_rate_estimate;
	int64_t		next_loss_update_ms;
}bbr_loss_rate_filter_t;

void			bbr_loss_filter_init(bbr_loss_rate_filter_t* filter);

void			bbr_loss_filter_update(bbr_loss_rate_filter_t* filter, int64_t feedback_ts, int packets_sent, int packets_lost);
double			bbr_loss_filter_get(bbr_loss_rate_filter_t* filter);

#endif



