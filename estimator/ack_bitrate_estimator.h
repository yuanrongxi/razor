/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __ack_bitrate_estimator_h_
#define __ack_bitrate_estimator_h_

#include "estimator_common.h"

/*通过接收端反馈的收包信息来估算本端发送的有效码率*/
typedef struct
{
	int64_t			alr_ended_ts;

	int64_t			curr_win_ms;
	int64_t			prev_ts;

	int				sum;
	float			bitrate_estimate;
	float			bitrate_estimate_var;
}ack_bitrate_estimator_t;

ack_bitrate_estimator_t*	ack_estimator_create();
void						ack_estimator_destroy(ack_bitrate_estimator_t* est);

void						ack_estimator_reset(ack_bitrate_estimator_t* est);

void						ack_estimator_incoming(ack_bitrate_estimator_t* est, packet_feedback_t packets[], int size);
uint32_t					ack_estimator_bitrate_bps(ack_bitrate_estimator_t* est);

void						ack_estimator_set_alrended(ack_bitrate_estimator_t* est, int64_t ts);

#endif
