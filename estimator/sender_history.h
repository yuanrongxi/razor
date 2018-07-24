/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sender_history_h_
#define __sender_history_h_

#include "estimator_common.h"
#include "cf_platform.h"
#include "cf_skiplist.h"
#include "cf_unwrapper.h"

typedef struct
{
	uint32_t		limited_ms;

	cf_unwrapper_t	wrapper;
	int64_t			last_ack_seq_num;
	skiplist_t*		l;
	size_t			outstanding_bytes;
}sender_history_t;

sender_history_t*	sender_history_create(uint32_t limited_ms);
void				sender_history_destroy(sender_history_t* hist);

void				sender_history_add(sender_history_t* hist, packet_feedback_t* packet);
int					sender_history_get(sender_history_t* hist, uint16_t seq, packet_feedback_t* packet, int remove_flag);

size_t				sender_history_outstanding_bytes(sender_history_t* hist);
#endif
