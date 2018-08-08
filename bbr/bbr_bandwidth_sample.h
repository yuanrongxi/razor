/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_bandwidth_sample_h_
#define __bbr_bandwidth_sample_h_

#include "cf_platform.h"

typedef struct
{
	int32_t		bandwidth;
	int64_t		rtt;
	int			is_app_limited;
}bbr_bandwidth_sample_t;

typedef struct
{
	int64_t			send_time;
	size_t			size;

	size_t			total_data_sent;
	size_t			total_data_acked_at_the_last_acked_packet;
	size_t			total_data_sent_at_last_acked_packet;

	int64_t			last_acked_packet_sent_time;
	int64_t			last_acked_packet_ack_time;

	int				is_app_limited;
	int				ignore;
}bbr_packet_point_t;

/*定义一个通过记录sent和acked报文轨迹来统计带宽的跟踪器*/
typedef struct
{
	int32_t				rate_bps;
	size_t				total_data_sent;
	size_t				total_data_acked;
	size_t				total_data_sent_at_last_acked_packet;
	
	int64_t				last_acked_packet_sent_time;
	int64_t				last_acked_packet_ack_time;
	int64_t				last_sent_packet;

	int					is_app_limited;
	int64_t				end_of_app_limited_phase;

	int					size;
	int					count;
	int64_t				start_pos;
	int64_t				index;
	bbr_packet_point_t*	points;
}bbr_bandwidth_sampler_t;

bbr_bandwidth_sampler_t*	sampler_create();
void						sampler_destroy(bbr_bandwidth_sampler_t* sampler);
void						sampler_reset(bbr_bandwidth_sampler_t* sampler);

void						sampler_on_packet_sent(bbr_bandwidth_sampler_t* sampler, int64_t sent_time, int64_t packet_number, size_t data_size, size_t data_in_flight);
bbr_bandwidth_sample_t		sampler_on_packet_acked(bbr_bandwidth_sampler_t* sampler, int64_t ack_time, int64_t number);
void						sampler_on_packet_lost(bbr_bandwidth_sampler_t* sampler, int64_t packet_number);
void						sampler_on_app_limited(bbr_bandwidth_sampler_t* sampler);
void						sampler_remove_old(bbr_bandwidth_sampler_t* sampler, int64_t least_unacked);
size_t						sampler_total_data_acked(bbr_bandwidth_sampler_t* sampler);
int							sampler_is_app_limited(bbr_bandwidth_sampler_t* sampler);
int64_t						sampler_end_of_app_limited_phase(bbr_bandwidth_sampler_t* sampler);
int							sampler_get_sample_num(bbr_bandwidth_sampler_t* sampler);
size_t						sampler_get_data_size(bbr_bandwidth_sampler_t* sampler, int64_t number);

#endif




