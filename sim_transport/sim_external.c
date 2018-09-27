/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_internal.h"

static sim_session_t* g_session = NULL;
static sim_log_fn g_log_func = NULL;
static int g_inited = 0;

void ex_sim_log(int level, const char* file, int line, const char *fmt, ...)
{
	if (g_log_func == NULL) {
		return;
	}

	va_list vl;
	va_start(vl, fmt);
	g_log_func(level, file, line, fmt, vl);
	va_end(vl);
}

void sim_init(uint16_t port, void* event, sim_log_fn log_cb, sim_notify_fn notify_cb, sim_change_bitrate_fn change_bitrate_cb, sim_state_fn state_cb)
{
	if (g_inited != 0)
		return;

	su_platform_init();
	g_inited = 1;

	g_log_func = log_cb;

	/*初始化razor的日志接口*/
	razor_setup_log(log_cb);

	/*创建session*/
	g_session = sim_session_create(port, event, notify_cb, change_bitrate_cb, state_cb);
}

void sim_destroy()
{
	if (g_inited == 0)
		return;

	if (g_session != NULL){
		sim_session_destroy(g_session);
		g_session = NULL;
	}

	su_platform_uninit();

	g_inited = 0;
}

int sim_connect(uint32_t local_uid, const char* peer_ip, uint16_t peer_port, int transport_type, int padding)
{
	if (g_inited == 0)
		return -1;

	return sim_session_connect(g_session, local_uid, peer_ip, peer_port, transport_type, padding);
}

int sim_disconnect()
{
	if (g_inited == 0)
		return -1;

	return sim_session_disconnect(g_session);
}

void sim_set_bitrates(uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate)
{
	if (g_inited == 0)
		return;

	sim_session_set_bitrates(g_session, min_bitrate, start_bitrate, max_bitrate);
}

int sim_send_video(uint8_t payload_type, uint8_t ftype, const uint8_t* data, size_t size)
{
	if (g_inited == 0)
		return -1;

	return sim_session_send_video(g_session, payload_type, ftype, data, size);
}

int sim_recv_video(uint8_t* data, size_t* sizep, uint8_t* payload_type)
{
	if (g_inited == 0)
		return -1;

	return sim_session_recv_video(g_session, data, sizep, payload_type);
}






