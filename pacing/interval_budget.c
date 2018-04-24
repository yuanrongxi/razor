/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "interval_budget.h"

#define k_window_ms 500
#define k_delta_ms  2000

void init_interval_budget(interval_budget_t* budget, int initial_target_rate_kbps, int can_build_up_underuse)
{
	budget->bytes_remaining = 0;
	budget->can_build_up_underuse = can_build_up_underuse;
	set_target_rate_kbps(budget, initial_target_rate_kbps);
}

void set_target_rate_kbps(interval_budget_t* budget, int target_rate_kbps)
{
	budget->target_rate_kbps = target_rate_kbps;
	budget->max_bytes_in_budget = (k_window_ms * target_rate_kbps) / 8;
	budget->bytes_remaining = SU_MIN(SU_MAX(-budget->max_bytes_in_budget, budget->bytes_remaining), budget->max_bytes_in_budget);
}

void increase_budget(interval_budget_t* budget, int delta_ts)
{
	int bytes = budget->target_rate_kbps * delta_ts / 8;
	if (budget->bytes_remaining < 0 || budget->can_build_up_underuse == 0) /*累计计算*/
		budget->bytes_remaining = SU_MIN(budget->bytes_remaining + bytes, budget->max_bytes_in_budget);
	else/*重新计算*/
		budget->bytes_remaining = SU_MIN(bytes, budget->max_bytes_in_budget);
}

void use_budget(interval_budget_t* budget, int bytes)
{
	budget->bytes_remaining -= bytes;
	budget->bytes_remaining = SU_MAX(-budget->max_bytes_in_budget, budget->bytes_remaining);
}

size_t budget_remaining(interval_budget_t* budget)
{
	return budget->bytes_remaining > 0 ? budget->bytes_remaining : 0;
}

int budget_level_precent(interval_budget_t* budget)
{
	return budget->bytes_remaining * 100 / budget->max_bytes_in_budget;
}





