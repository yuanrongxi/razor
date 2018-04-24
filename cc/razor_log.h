/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __razor_log_h_
#define __razor_log_h_

#include <stdio.h>
#include <stdlib.h>
#include "razor_api.h"

void ex_razor_log(int level, const char* file, int line, const char *fmt, ...);

#define razor_debug(...)  				ex_razor_log(0, __FILE__, __LINE__, __VA_ARGS__)
#define razor_info(...)    				ex_razor_log(1, __FILE__, __LINE__, __VA_ARGS__)
#define razor_warn(...)					ex_razor_log(2, __FILE__, __LINE__, __VA_ARGS__)
#define razor_error(...)   				ex_razor_log(3, __FILE__, __LINE__, __VA_ARGS__)


#endif
