/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sim_internal_h_
#define __sim_internal_h_

#include "cf_platform.h"
#include "cf_skiplist.h"
#include "sim_proto.h"
#include "cf_stream.h"
#include "razor_api.h"			/*razor的引用头文件*/
#include "rate_stat.h"
#include "sim_external.h"

struct __sim_session;
typedef struct __sim_session			sim_session_t;
struct __sim_sender;
typedef struct __sim_sender				sim_sender_t;
struct __sim_receiver;
typedef struct __sim_receiver			sim_receiver_t;

#include "sim_session.h"
#include "sim_sender.h"
#include "sim_receiver.h"


#define MIN_BITRATE		80000					/*10KB*/
#define MAX_BITRATE		16000000				/*2MB*/
#define START_BITRATE	800000					/*100KB*/

void					free_video_seg(skiplist_item_t key, skiplist_item_t val, void* args);

/****************************************************************************************************/
void					sim_limiter_init(sim_sender_limiter_t* limiter, int windows_size_ms);
void					sim_limiter_destroy(sim_sender_limiter_t* limiter);
void					sim_limiter_set_max_bitrate(sim_sender_limiter_t* limiter, uint32_t bitrate);
int						sim_limiter_try(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts);
void					sim_limiter_update(sim_sender_limiter_t* limiter, size_t size, int64_t now_ts);

/****************************************************************************************************/
sim_sender_t*			sim_sender_create(sim_session_t* s, int transport_type, int padding);
void					sim_sender_destroy(sim_session_t* s, sim_sender_t* sender);
void					sim_sender_reset(sim_session_t* s, sim_sender_t* sender, int transport_type, int padding);
int						sim_sender_active(sim_session_t* s, sim_sender_t* sender);

int						sim_sender_put(sim_session_t* s, sim_sender_t* sender, uint8_t payload_type, uint8_t ftype, const uint8_t* data, size_t size);
int						sim_sender_ack(sim_session_t* s, sim_sender_t* sender, sim_segment_ack_t* ack);
void					sim_sender_timer(sim_session_t* s, sim_sender_t* sender, uint64_t now_ts);
void					sim_sender_update_rtt(sim_session_t* s, sim_sender_t* r);
void					sim_sender_feedback(sim_session_t* s, sim_sender_t* sender, sim_feedback_t* feedback);
void					sim_sender_set_bitrates(sim_session_t* s, sim_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);
/***************************************************************************************************/

sim_receiver_t*			sim_receiver_create(sim_session_t* s, int transport_type);
void					sim_receiver_destroy(sim_session_t* s, sim_receiver_t* r);
void					sim_receiver_reset(sim_session_t* s, sim_receiver_t* r, int transport_type);
int						sim_receiver_active(sim_session_t* s, sim_receiver_t* r, uint32_t uid);
int						sim_receiver_put(sim_session_t* s, sim_receiver_t* r, sim_segment_t* seg);
int						sim_receiver_padding(sim_session_t* s, sim_receiver_t* r, uint16_t transport_seq, uint32_t send_ts, size_t data_size);
int						sim_receiver_get(sim_session_t* s, sim_receiver_t* r, uint8_t* data, size_t* sizep, uint8_t* payload_type);
void					sim_receiver_timer(sim_session_t* s, sim_receiver_t* r, int64_t now_ts);
void					sim_receiver_update_rtt(sim_session_t* s, sim_receiver_t* r);
uint32_t				sim_receiver_cache_delay(sim_session_t* s, sim_receiver_t* r);
/**************************************************************************************************/

sim_session_t*			sim_session_create(uint16_t port, void* event, sim_notify_fn notify_cb, sim_change_bitrate_fn change_bitrate_cb, sim_state_fn state_cb);
void					sim_session_destroy(sim_session_t* s);

/*连接一个接收端*/
int						sim_session_connect(sim_session_t* s, uint32_t local_uid, const char* peer_ip, uint16_t peer_port, int transport_type, int padding);
/*断开一个接收端*/
int						sim_session_disconnect(sim_session_t* s);
/*发送视频数据*/
int						sim_session_send_video(sim_session_t* s, uint8_t payload_type, uint8_t ftype, const uint8_t* data, size_t size);
/*获取接收端的视频数据*/
int						sim_session_recv_video(sim_session_t* s, uint8_t* data, size_t* sizep, uint8_t* payload_type);

/*设置码率范围和当前使用的码率，一般只在视频编码器初始化的时候调用*/
void					sim_session_set_bitrates(sim_session_t* s, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

/*内部发送消息接口*/
int						sim_session_network_send(sim_session_t* s, bin_stream_t* strm);
void					sim_session_calculate_rtt(sim_session_t* s, uint32_t keep_rtt);

/*******************************************************************************************/
void ex_sim_log(int level, const char* file, int line, const char *fmt, ...);

#define sim_debug(...)  				ex_sim_log(0, __FILE__, __LINE__, __VA_ARGS__)
#define sim_info(...)    				ex_sim_log(1, __FILE__, __LINE__, __VA_ARGS__)
#define sim_warn(...)					ex_sim_log(2, __FILE__, __LINE__, __VA_ARGS__)
#define sim_error(...)   				ex_sim_log(3, __FILE__, __LINE__, __VA_ARGS__)

#define msg_log(addr, ...)						\
do{												\
	char ip[IP_SIZE] = { 0 };					\
	su_addr_to_string((addr), ip, IP_SIZE);		\
	sim_info(__VA_ARGS__);						\
} while (0)

#endif



