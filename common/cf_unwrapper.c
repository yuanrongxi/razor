/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "cf_unwrapper.h"
#include <assert.h>

#define UINT32_MAX_VAL 0XFFFFFFFF
#define UINT16_MAX_VAL 0XFFFF

#define IS_NEWER(type, bp, val, prev_val) do{					\
	if (val - prev_val == (bp))									\
		return (val > prev_val ? 0 : -1);						\
	if (val != prev_val && ((type)(val - prev_val)) < (bp))		\
		return 0;												\
	return -1;													\
}while (0)

void init_unwrapper16(cf_unwrapper_t* wrap)
{
	wrap->size = sizeof(uint16_t);
	wrap->last_value = 0;
}

static int wrap16_is_newer(uint16_t val, uint16_t prev_val)
{
	IS_NEWER(uint16_t, (UINT16_MAX_VAL >> 1) + 1, val, prev_val);
}

static int64_t wrap16_update(cf_unwrapper_t* wrap, uint16_t val)
{
	int64_t max_plus = (int64_t)UINT16_MAX_VAL + 1;

	uint16_t cropped_last = (uint16_t)(wrap->last_value);
	int64_t delta = val - cropped_last;
	if (wrap16_is_newer(val, cropped_last) == 0){
		if (delta < 0)
			delta += max_plus;
	}
	else if (delta > 0 && wrap->last_value + delta - max_plus >= 0){
		delta -= max_plus;
	}

	return wrap->last_value + delta;
}

int64_t wrap_uint16(cf_unwrapper_t* wrap, uint16_t val)
{
	assert(wrap->size == sizeof(uint16_t));

	wrap->last_value = wrap16_update(wrap, val);
	return wrap->last_value;
}

void init_unwrapper32(cf_unwrapper_t* wrap)
{
	wrap->size = sizeof(uint32_t);
	wrap->last_value = 0;
}

static int wrap32_is_newer(uint32_t val, uint32_t prev_val)
{
	IS_NEWER(uint32_t, (UINT32_MAX_VAL >> 1) + 1, val, prev_val);
}

static int64_t wrap32_update(cf_unwrapper_t* wrap, uint32_t val)
{
	int64_t max_plus = (int64_t)UINT32_MAX_VAL + 1;

	uint32_t cropped_last = (uint32_t)(wrap->last_value);
	int64_t delta = val - cropped_last;
	if (wrap32_is_newer(val, cropped_last) == 0){
		if (delta < 0)
			delta += max_plus;
	}
	else if (delta > 0 && wrap->last_value + delta - max_plus >= 0){
		delta -= max_plus;
	}

	return wrap->last_value + delta;
}

int64_t wrap_uint32(cf_unwrapper_t* wrap, uint32_t val)
{
	assert(wrap->size == sizeof(uint32_t));
	wrap->last_value = wrap32_update(wrap, val);
	return wrap->last_value;
}



