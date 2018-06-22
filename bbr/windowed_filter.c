/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "windowed_filter.h"

int max_val_func(int64_t v1, int64_t v2)
{
	if (v1 >= v2)
		return 1;
	else
		return 0;
}

int min_val_func(int64_t v1, int64_t v2)
{
	if (v1 <= v2)
		return 1;
	else
		return 0;
}

void wnd_filter_init(windowed_filter_t* filter, int64_t wnd_size, compare_func_f comp)
{
	filter->wnd_size = wnd_size;
	filter->comp_func = comp;

	wnd_filter_reset(filter, 0, 0);
}

void wnd_filter_reset(windowed_filter_t* filter, int64_t new_sample, int64_t new_ts)
{
	int i;
	for (i = 0; i < 3; i++){
		filter->estimates[i].sample = new_sample;
		filter->estimates[i].ts = new_ts;
	}
}

void wnd_filter_set_window_size(windowed_filter_t* filter, int64_t wnd_size)
{
	filter->wnd_size = wnd_size;
}

void wnd_filter_update(windowed_filter_t* filter, int64_t new_sample, int64_t new_ts)
{
	wnd_sample_t sample = { new_sample, new_ts };

	/*如果这是第一个更新、filter中记录的值都过了时间窗或者是最大的值，用新的值进行覆盖更新*/
	if (filter->estimates[0].sample == 0 || filter->comp_func(new_sample, filter->estimates[0].sample) > 0
		|| new_ts - filter->estimates[2].ts > filter->wnd_size){
		wnd_filter_reset(filter, new_sample, new_ts);
		return;
	}

	/*新更新的值是第二大的值，对1，2位的值进行更新*/
	if (filter->comp_func(new_sample, filter->estimates[1].sample) > 0){
		filter->estimates[1] = sample;
		filter->estimates[2] = sample;
	}
	else if (filter->comp_func(new_sample, filter->estimates[2].sample) > 0){/*更新的值为第三大，只对末尾的2进行更新*/
		filter->estimates[2] = sample;
	}

	/*进行过期淘汰,先判断0位是否淘汰，再判断1是否淘汰,因为2位上在第一个if上做了判断*/
	if (new_ts - filter->estimates[0].ts > filter->wnd_size){
		filter->estimates[0] = filter->estimates[1];
		filter->estimates[1] = filter->estimates[2];
		filter->estimates[2] = sample;

		/*再次对移动后的filter进行过期淘汰判断*/
		if (new_ts - filter->estimates[0].ts > filter->wnd_size){
			filter->estimates[0] = filter->estimates[1];
			filter->estimates[1] = filter->estimates[2];
		}

		return;
	}

	/*按时间戳先后进行相同值的更新*/
	if (filter->estimates[0].sample == filter->estimates[1].sample
		&& new_ts - filter->estimates[1].ts > (filter->wnd_size >> 2)){/*0号位上最大的值和1号位上的值相等，但1号位的时间戳点距离当前时间点超过窗口的1/4,意味着1 2号位的需要更新到最新值上来*/
		filter->estimates[1] = filter->estimates[2] = sample;
	}

	/*同上原理，对2号位上的值进行更新*/
	if (filter->estimates[1].sample == filter->estimates[2].sample
		&& new_ts - filter->estimates[2].ts > (filter->wnd_size >> 1))
		filter->estimates[2] = sample;
}

int64_t wnd_filter_best(windowed_filter_t* filter)
{
	return filter->estimates[0].sample;
}

int64_t wnd_filter_second_best(windowed_filter_t* filter)
{
	return filter->estimates[1].sample;
}

int64_t wnd_filter_third_best(windowed_filter_t* filter)
{
	return filter->estimates[2].sample;
}




