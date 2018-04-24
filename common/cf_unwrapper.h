/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __cf_unwrapper_h_
#define __cf_unwrapper_h_

#include <stdint.h>

/*一个循环的整数序列，例如一个uint16_t类型的seq,增到65535后就回到0了，用这个结构可以表示永远的递增*/

typedef struct
{
	int		size;
	int64_t last_value;		/*类型当前值*/
}cf_unwrapper_t;

void		init_unwrapper16(cf_unwrapper_t* wrap);
int64_t		wrap_uint16(cf_unwrapper_t* wrap, uint16_t val);

void		init_unwrapper32(cf_unwrapper_t* wrap);
int64_t		wrap_uint32(cf_unwrapper_t* wrap, uint32_t val);

#endif
