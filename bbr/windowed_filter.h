/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __windowed_filter_h_
#define __windowed_filter_h_

#include "cf_platform.h"

typedef struct
{
	int64_t			sample;
	int64_t			ts;
}wnd_sample_t;

typedef int(*compare_func_f)(int64_t v1, int64_t v2);

typedef struct
{
	int64_t			wnd_size;
	wnd_sample_t	estimates[3];
	compare_func_f  comp_func;
}windowed_filter_t;

int					max_val_func(int64_t v1, int64_t v2);
int					min_val_func(int64_t v1, int64_t v2);

void				wnd_filter_init(windowed_filter_t* filter, int64_t wnd_size, compare_func_f comp);
void				wnd_filter_reset(windowed_filter_t* filter, int64_t new_sample, int64_t new_ts);
void				wnd_filter_set_window_size(windowed_filter_t* filter, int64_t wnd_size);

void				wnd_filter_update(windowed_filter_t* filter, int64_t new_sample, int64_t new_ts);

int64_t				wnd_filter_best(windowed_filter_t* filter);
int64_t				wnd_filter_second_best(windowed_filter_t* filter);
int64_t				wnd_filter_third_best(windowed_filter_t* filter);

#endif



