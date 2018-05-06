/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "razor_log.h"
#include "razor_api.h"
#include "cf_platform.h"

static razor_log_func g_log_cb = NULL;

void ex_razor_log(int level, const char* file, int line, const char *fmt, ...)
{
	va_list vl;
	if (g_log_cb != NULL) {
		va_start(vl, fmt);
		g_log_cb(level, file, line, fmt, vl);
		va_end(vl);
	}
}

void razor_setup_log(razor_log_func log_cb)
{
	g_log_cb = log_cb;
}


