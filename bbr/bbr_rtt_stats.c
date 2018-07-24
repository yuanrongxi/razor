/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "bbr_rtt_stats.h"

#define  kInitialRttMs 100
#define  kAlpha 0.125
#define  kOneMinusAlpha (1 - kAlpha)
#define  kBeta 0.25
#define  kOneMinusBeta (1 - kBeta)
#define  kNumMicrosPerMilli 1000

void bbr_rtt_init(bbr_rtt_stat_t* s)
{
	s->latest_rtt = 0;
	s->min_rtt = 0;
	s->smoothed_rtt = 100;
	s->previous_srtt = 20;
	s->mean_deviation = 20;
	s->initial_rtt_us = kNumMicrosPerMilli * kInitialRttMs;
}

void bbr_rtt_update(bbr_rtt_stat_t* s, int64_t send_delta, int64_t ack_delay, int64_t now_ts)
{
	int64_t rtt_sample;
	if (send_delta <= 0)
		return;

	if (s->min_rtt == 0 || s->min_rtt > send_delta)
		s->min_rtt = SU_MAX(send_delta, 4);

	rtt_sample = SU_MAX(10, send_delta);
	s->previous_srtt = s->smoothed_rtt;
	if (rtt_sample > ack_delay){
		rtt_sample = rtt_sample - ack_delay;
	}

	s->latest_rtt = rtt_sample;

	if (s->smoothed_rtt == 0){
		s->smoothed_rtt = rtt_sample;
		s->mean_deviation = rtt_sample / 2;
	}
	else{
		s->mean_deviation = kOneMinusBeta * s->mean_deviation + kBeta * SU_ABS(s->smoothed_rtt, s->latest_rtt);
		s->smoothed_rtt = kOneMinusAlpha * s->smoothed_rtt + kAlpha * rtt_sample;
	}
}

void bbr_rtt_expire_smoothed_metrics(bbr_rtt_stat_t* s)
{
	int64_t abs = SU_ABS(s->smoothed_rtt, s->latest_rtt);
	s->mean_deviation = SU_MAX(s->mean_deviation, abs);
	s->smoothed_rtt = SU_MAX(s->smoothed_rtt, s->latest_rtt);
}

void bbr_rtt_connection_migration(bbr_rtt_stat_t* s)
{
	s->latest_rtt = 0;
	s->min_rtt = 0;
	s->smoothed_rtt = 0;
	s->mean_deviation = 0;
	s->initial_rtt_us = kNumMicrosPerMilli * kInitialRttMs;
}

int64_t	bbr_smoothed_rtt(bbr_rtt_stat_t* s)
{
	return s->smoothed_rtt;
}

int64_t	bbr_previous_srtt(bbr_rtt_stat_t* s)
{
	return s->previous_srtt;
}

int64_t	bbr_initial_rtt_us(bbr_rtt_stat_t* s)
{
	return s->initial_rtt_us;
}

void bbr_set_initial_rtt_us(bbr_rtt_stat_t* s, int64_t initial_rtt_us)
{
	s->initial_rtt_us = initial_rtt_us;
}

int64_t	bbr_mean_deviation(bbr_rtt_stat_t* s)
{
	return s->mean_deviation;
}

int64_t bbr_latest_rtt(bbr_rtt_stat_t* s)
{
	return s->latest_rtt;
}

int64_t bbr_min_rtt(bbr_rtt_stat_t* s)
{
	return s->min_rtt;
}