/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sender_bandwidth_estimator_h_
#define __sender_bandwidth_estimator_h_

#include "estimator_common.h"
#include "cf_platform.h"

#define LOSS_WND_SIZE 20
typedef struct
{
	int				frags[LOSS_WND_SIZE];
	int				index;

	int				acc;
	int				count;

	double			slope;
}slope_filter_t;

typedef struct
{
	int64_t ts;
	uint32_t bitrate;
}min_bitrate_t;

#define MIN_HISTORY_ARR_SIZE  128

typedef struct
{
	uint32_t				curr_bitrate;
	uint32_t				min_conf_bitrate;
	uint32_t				max_conf_bitrate;

	int64_t					last_feelback_ts;
	int64_t					last_packet_report_ts;
	int64_t					last_timeout_ts;

	uint32_t				last_rtt;
	uint8_t					last_fraction_loss;
	int8_t					has_decreased_since_last_fraction_loss; 

	uint32_t				bwe_incoming;			/*REMB*/
	uint32_t				delay_base_bitrate;		/*sender delay estimator's bandwidth*/
	int						state;

	int64_t					last_decrease_ts;
	int64_t					first_report_ts;
	int						initially_lost_packets;
	int						bitrate_2_seconds_kbps;

	int						lost_packets_since_last_loss_update_Q8;
	int						expected_packets_since_last_loss_update;

	float					low_loss_threshold;
	float					high_loss_threshold;
	uint32_t				bitrate_threshold;

	/*一个一秒钟之内带宽最小值历史记录*/
	uint32_t				begin_index;
	uint32_t				end_index;
	min_bitrate_t			min_bitrates[MIN_HISTORY_ARR_SIZE];

	int						prev_fraction_loss;
	slope_filter_t			slopes;
}sender_estimation_t;

sender_estimation_t*		sender_estimation_create(uint32_t min_bitrate, uint32_t max_bitrate);
void						sender_estimation_destroy(sender_estimation_t* estimation);

void						sender_estimation_update(sender_estimation_t* estimation, int64_t cur_ts, uint32_t acked_bitrate);
void						sender_estimation_update_remb(sender_estimation_t* estimation, int64_t cur_ts, uint32_t bitrate);
void						sender_estimation_update_delay_base(sender_estimation_t* estimation, int64_t cur_ts, uint32_t bitrate, int state);

void						sender_estimation_update_block(sender_estimation_t* estimation, uint8_t fraction_loss, uint32_t rtt, int number_of_packets, int64_t cur_ts, uint32_t acked_bitrate);

void						sender_estimation_set_bitrates(sender_estimation_t* estimation, uint32_t send_bitrate, uint32_t min_bitrate, uint32_t max_bitrate);
void						sender_estimation_set_send_bitrate(sender_estimation_t* estimation, uint32_t send_bitrate);
void						sender_estimation_set_minmax_bitrate(sender_estimation_t* estimation, uint32_t min_bitrate, uint32_t max_bitrate);

uint32_t					sender_estimation_get_min_bitrate(sender_estimation_t* estimation);

#endif



