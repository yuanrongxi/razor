#ifndef __sim_internal_h_
#define __sim_internal_h_

#include "cf_platform.h"
#include "cf_skiplist.h"
#include "sim_proto.h"
#include "cf_stream.h"
#include "razor_api.h"			/*razor的引用头文件*/

struct __sim_session;
typedef struct __sim_session			sim_session_t;
struct __sim_sender;
typedef struct __sim_sender				sim_sender_t;
struct __sim_receiver;
typedef struct __sim_receiver			sim_receiver_t;

#include "sim_session.h"
#include "sim_sender.h"
#include "sim_receiver.h"

void					free_video_seg(skiplist_item_t key, skiplist_item_t val, void* args);
/****************************************************************************************************/
sim_sender_t*			sim_sender_create(sim_session_t* s);
void					sim_sender_destroy(sim_session_t* s, sim_sender_t* sender);
void					sim_sender_reset(sim_session_t* s, sim_sender_t* sender);
int						sim_sender_active(sim_session_t* s, sim_sender_t* sender);

int						sim_sender_put(sim_session_t* s, sim_sender_t* sender, uint8_t ftype, const uint8_t* data, size_t size);
int						sim_sender_ack(sim_session_t* s, sim_sender_t* sender, uint32_t acked_packet_id, uint16_t nacks[], int nacks_num);
void					sim_sender_timer(sim_session_t* s, sim_sender_t* sender, uint64_t cur_ts);
/***************************************************************************************************/

sim_receiver_t*			sim_receiver_create(sim_session_t* s);
void					sim_receiver_destroy(sim_session_t* s, sim_receiver_t* r);
void					sim_receiver_reset(sim_session_t* s, sim_receiver_t* r);
int						sim_receiver_active(sim_session_t* s, sim_receiver_t* r);
int						sim_receiver_put(sim_session_t* s, sim_receiver_t* r, sim_segment_t* seg);
int						sim_receiver_get(sim_session_t* s, sim_receiver_t* r, uint8_t* data, size_t* sizep);
void					sim_receiver_timer(sim_session_t* s, sim_receiver_t* r);
/**************************************************************************************************/

sim_session_t*			sim_session_create(uint32_t uid);
void					sim_session_destroy(sim_session_t* s);

/*连接一个接收端*/
int						sim_session_connect(sim_session_t* s, const char* peer_id, uint16_t peer_port);
/*断开一个接收端*/
int						sim_session_disconnect(sim_session_t* s);
/*发送视频数据*/
int						sim_session_send(sim_session_t* s, uint8_t ftype, const uint8_t* data, size_t size);

/*获取接收端的视频数据*/
int						sim_session_recv(sim_session_t* s, uint32_t uid, uint8_t* data, size_t* sizep);

/*内部发送消息接口*/
int						sim_session_send_segment(sim_session_t* s, sim_segment_t* seg);
int						sim_session_send_segment_ack(sim_session_t* s, sim_segment_ack_t* ack);
int						sim_session_send_feedback(sim_session_t* s, sim_feedback_t* feedback);

#endif



