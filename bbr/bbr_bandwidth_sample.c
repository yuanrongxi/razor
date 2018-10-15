/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_bandwidth_sample.h"
#include "razor_log.h"

#define kMaxTrackedPackets  10000
#define kDefaultPoints		1024

static inline void sampler_to_point(bbr_bandwidth_sampler_t* sampler, bbr_packet_point_t* point)
{
	point->total_data_sent = sampler->total_data_sent;
	point->total_data_acked_at_the_last_acked_packet = sampler->total_data_acked;
	point->total_data_sent_at_last_acked_packet = sampler->total_data_sent_at_last_acked_packet;

	point->last_acked_packet_ack_time = sampler->last_acked_packet_ack_time;
	point->last_acked_packet_sent_time = sampler->last_acked_packet_sent_time;

	point->is_app_limited = sampler->is_app_limited;

	point->ignore = 0;
}

bbr_bandwidth_sampler_t* sampler_create()
{
	bbr_bandwidth_sampler_t* sampler = calloc(1, sizeof(bbr_bandwidth_sampler_t));

	sampler->size = kDefaultPoints;
	sampler->points = malloc(sampler->size * sizeof(bbr_packet_point_t));

	sampler_reset(sampler);

	return sampler;
}

void sampler_destroy(bbr_bandwidth_sampler_t* sampler)
{
	if (sampler != NULL){
		free(sampler->points);
		free(sampler);
	}
}

void sampler_reset(bbr_bandwidth_sampler_t* sampler)
{
	int i;
	bbr_packet_point_t* point;

	sampler->total_data_sent = 0;
	sampler->total_data_acked = 0;
	sampler->total_data_sent_at_last_acked_packet = 0;
	sampler->last_acked_packet_sent_time = -1;
	sampler->last_acked_packet_ack_time = -1;
	sampler->last_sent_packet = 0;
	sampler->rate_bps = -1;

	sampler->is_app_limited = 0;
	sampler->end_of_app_limited_phase = 0;

	sampler->start_pos = -1;
	sampler->index = -1;
	sampler->count = 0;
	for (i = 0; i < sampler->size; ++i){
		point = &sampler->points[i];

		point->send_time = 0;
		point->size = 0;

		point->total_data_sent = 0;
		point->total_data_acked_at_the_last_acked_packet = 0;
		point->total_data_sent_at_last_acked_packet = 0;

		point->last_acked_packet_ack_time = -1;
		point->last_acked_packet_sent_time = -1;
		point->is_app_limited = 0;
		point->ignore = 1;
	}
}

static void sampler_resize_points(bbr_bandwidth_sampler_t* sampler, int size)
{
	int64_t i;
	int old_pos, new_pos, new_size;
	bbr_packet_point_t* new_points;

	if (size <= sampler->size)
		return;

	new_size = sampler->size;
	while (size > new_size)
		new_size *= 2;

	new_points = (bbr_packet_point_t*)malloc(new_size * sizeof(bbr_packet_point_t));
	for (i = sampler->start_pos; i < sampler->index; ++i){
		old_pos = i % sampler->size;
		new_pos = i % new_size;
		new_points[new_pos] = sampler->points[old_pos];
	}

	free(sampler->points);
	sampler->points = new_points;
	sampler->size = new_size;
}

static void sampler_add_point(bbr_bandwidth_sampler_t* sampler, int64_t sent_time, int64_t number, size_t data_size)
{
	bbr_packet_point_t* point;
	int pos;

	if (number <= sampler->start_pos)
		return;

	if (sampler->start_pos == -1)
		sampler->start_pos = number;

	/*超出范围，进行扩大*/
	if (sampler->size + sampler->start_pos < number)
		sampler_resize_points(sampler, (int)(number - sampler->start_pos));
	
	sampler->index = number;

	pos = sampler->index % sampler->size;
	point = &sampler->points[pos];
	point->size = data_size;
	point->send_time = sent_time;

	sampler_to_point(sampler, point);

	sampler->count++;
}

static void sampler_remove_point(bbr_bandwidth_sampler_t* sampler, int64_t number)
{
	bbr_packet_point_t* point;
	int pos;

	if (sampler->start_pos > number || sampler->index < number)
		return;

	pos = number % sampler->size;
	point = &sampler->points[pos];
	point->ignore = 1;

	/*进行数据抹除*/
	if (sampler->start_pos == number){
		point->send_time = 0;
		point->size = 0;
		point->total_data_sent = 0;
		point->total_data_acked_at_the_last_acked_packet = 0;
		point->total_data_sent_at_last_acked_packet = 0;
		point->last_acked_packet_ack_time = -1;
		point->last_acked_packet_sent_time = -1;
		point->is_app_limited = 0;

		if (sampler->index > sampler->start_pos)
			sampler->start_pos++;
	}

	if (sampler->count > 0)
		sampler->count--;
}

void sampler_on_packet_sent(bbr_bandwidth_sampler_t* sampler, int64_t sent_time, int64_t packet_number, size_t data_size, size_t data_in_flight)
{
	sampler->last_sent_packet = packet_number;
	sampler->total_data_sent += data_size;

	if (data_in_flight <= 0){
		sampler->last_acked_packet_ack_time = sent_time;
		sampler->last_acked_packet_sent_time = sent_time;

		sampler->total_data_sent_at_last_acked_packet = sampler->total_data_sent;
	}

	if (sampler->index >= 0 && sampler->index + kMaxTrackedPackets < packet_number)
		return;

	sampler_add_point(sampler, sent_time, packet_number, data_size);
}

static bbr_bandwidth_sample_t sampler_on_packet_acked_inner(bbr_bandwidth_sampler_t* sampler, int64_t ack_time, int64_t number, bbr_packet_point_t* point)
{
	int32_t send_rate, ack_rate;

	bbr_bandwidth_sample_t ret = { 0 };

	sampler->total_data_acked += point->size;
	sampler->total_data_sent_at_last_acked_packet = point->total_data_sent;
	sampler->last_acked_packet_ack_time = ack_time;
	sampler->last_acked_packet_sent_time = point->send_time;

	if (sampler->is_app_limited == 1 && number > sampler->end_of_app_limited_phase)
		sampler->is_app_limited = 0;

	if (point->last_acked_packet_ack_time == -1 || point->last_acked_packet_sent_time == -1)
		return ret;

	/*计算send_rate*/
	send_rate = 0x7fffffff;
	if (point->send_time > point->last_acked_packet_sent_time){
		//send_rate = (point->total_data_sent - point->total_data_sent_at_last_acked_packet) / ((int)(point->send_time - point->last_acked_packet_sent_time));
	}

	if (ack_time > point->last_acked_packet_ack_time + 5){
		ack_rate = (sampler->total_data_acked - point->total_data_acked_at_the_last_acked_packet) / ((int)(ack_time - point->last_acked_packet_ack_time));

		ret.bandwidth = SU_MIN(ack_rate, send_rate);
		ret.rtt = ack_time - point->send_time;
		ret.is_app_limited = point->is_app_limited;
	}
	else if (ack_time == point->last_acked_packet_ack_time){
		ack_rate = (sampler->total_data_acked - point->total_data_acked_at_the_last_acked_packet) / 5;

		ret.bandwidth = SU_MIN(ack_rate, send_rate);
		ret.rtt = ack_time - point->send_time;
		ret.is_app_limited = point->is_app_limited;
	}

	return ret;
}

bbr_bandwidth_sample_t sampler_on_packet_acked(bbr_bandwidth_sampler_t* sampler, int64_t ack_time, int64_t number)
{
	bbr_packet_point_t* point;
	int pos;
	bbr_bandwidth_sample_t ret = { 0 };

	if (sampler->start_pos > number || sampler->index < number)
		return ret;

	pos = number % sampler->size;
	point = &sampler->points[pos];
	if (point->ignore == 1)
		return ret;

	ret = sampler_on_packet_acked_inner(sampler, ack_time, number, point);
	sampler_remove_point(sampler, number);

	return ret;
}

void sampler_on_packet_lost(bbr_bandwidth_sampler_t* sampler, int64_t packet_number)
{
	sampler_remove_point(sampler, packet_number);
}

void sampler_on_app_limited(bbr_bandwidth_sampler_t* sampler)
{
	sampler->is_app_limited = 1;
	sampler->end_of_app_limited_phase = sampler->last_sent_packet;
}

void sampler_remove_old(bbr_bandwidth_sampler_t* sampler, int64_t least_unacked)
{
	while (sampler->start_pos < least_unacked
		&& sampler->start_pos < sampler->index){
		sampler_remove_point(sampler, sampler->start_pos);
	}
}

size_t sampler_total_data_acked(bbr_bandwidth_sampler_t* sampler)
{
	return sampler->total_data_acked;
}

int sampler_is_app_limited(bbr_bandwidth_sampler_t* sampler)
{
	return sampler->is_app_limited;
}

int64_t sampler_end_of_app_limited_phase(bbr_bandwidth_sampler_t* sampler)
{
	return sampler->last_sent_packet;
}

int sampler_get_sample_num(bbr_bandwidth_sampler_t* sampler)
{
	return sampler->count;
}

size_t sampler_get_data_size(bbr_bandwidth_sampler_t* sampler, int64_t number)
{
	size_t ret;
	bbr_packet_point_t* point;
	int pos;

	ret = 0;
	if (sampler->start_pos > number || sampler->index < number)
		return ret;

	pos = number % sampler->size;
	point = &sampler->points[pos];
	if (point->ignore == 0)
		ret = point->size;

	return ret;
}



