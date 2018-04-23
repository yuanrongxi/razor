#ifndef __sim_external_h_
#define __sim_external_h_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum{
	sim_connect_notify = 1000,
	sim_network_timout,
	sim_disconnect_notify,
	net_interrupt_notify,
	net_recover_notify,
};

typedef void(*sim_notify_fn)(int type, uint32_t val);
typedef int(*sim_log_fn)(int level, const char* fmt, va_list vl);
typedef void(*sim_change_bitrate_fn)(uint32_t bw, int flag);
typedef void(*sim_state_fn)(uint32_t rbw, uint32_t sbw);


#endif



