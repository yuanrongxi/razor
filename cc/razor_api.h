#ifndef __razor_api__001_h_
#define __razor_api__001_h_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/*接收端反馈信息发送函数，是将接收端的信息反馈到发送端上*/
typedef void(*send_feedback_func)(void* handler, const uint8_t* payload, int payload_size);

/*发送端带宽改变函数，trigger是通讯上层的码率控制分配对象，它可以配置各个模块的码率并调节对应视频编码器码率*/
typedef void(*bitrate_changed_func)(void* trigger, uint32_t bitrate, uint8_t fraction_loss, uint32_t rtt);

/*发送端pacer触发报文发送的回调函数，
	handler 是发送对象
	packet_id是报文的id号
	retrans是重发标志
	size是报文的长度
调用这个函数会通过seq在发送队列中找到对应的packet，并进行packet发送*/
typedef void(*pace_send_func)(void* handler, uint32_t packet_id, int retrans, size_t size);

/*********************************发送端****************************************/
typedef struct __razor_sender razor_sender_t;

/*发送端拥塞控制对象的心跳函数，最好是5到10毫秒调用一次*/
typedef void(*sender_heartbeat_func)(razor_sender_t* sender);

/*设置码率范围*/
typedef void(*sender_set_bitrates)(razor_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

/*发送端增加一个待发送的报文*/
typedef void(*sender_add_packet_func)(razor_sender_t* sender, uint32_t packet_id, int retrans, size_t size);

/*上层将一个报文发送到网络后需要将其传输的自增长seq id和报文大小传入拥塞控制对象中进行记录，以便做对照查询*/
typedef void(*sender_on_send_func)(razor_sender_t* sender, uint16_t transport_seq, size_t size);

/*接收端反馈过来的信息，输入到拥塞控制对象中进行相应处理*/
typedef void(*sender_on_feedback_func)(razor_sender_t* sender, uint8_t* feedback, int feedback_size);

/*更新网络rtt*/
typedef void(*sender_update_rtt_func)(razor_sender_t* sender, int32_t rtt);

/*获取发送拥塞对象中等待发送队列的延迟*/
typedef int(*sender_get_pacer_queue_ms_func)(razor_sender_t* sender);

/*获取发送第一个报文的时间戳*/
typedef int64_t(*sender_get_first_ts)(razor_sender_t* sender);

struct __razor_sender
{
	sender_heartbeat_func			heartbeat;
	sender_set_bitrates				set_bitrates;
	sender_add_packet_func			add_packet;
	sender_on_send_func				on_send;
	sender_on_feedback_func			on_feedback;
	sender_update_rtt_func			update_rtt;
	sender_get_pacer_queue_ms_func	get_pacer_queue_ms;
	sender_get_first_ts				get_first_timestamp;
};

/*创建一个发送端的拥塞控制对象*/
razor_sender_t*	razor_sender_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms);

/*销毁一个发送端的拥塞控制对象*/
void			razor_sender_destroy(razor_sender_t* sender);

/*************************************接收端********************************************/
typedef struct __razor_receiver razor_receiver_t;

/*接收端拥塞对象的心跳函数*/
typedef void(*receiver_heartbeat_func)(razor_receiver_t* receiver);

/*接收端接收到一个报文，进行拥塞计算, 
 transport_seq		发送的报文的自增通道序号
 timestamp			发送端的相对时间戳（通过视频时间戳和发送偏移时间戳来计算），
 size				报文数据大小（包含UDP头和应用协议的头大小）
 remb				是否采用remb方式计算码率，= 0表示用，其他值表示不用，这个值来自于接收到的报文头
 */
typedef void(*receiver_on_received_func)(razor_receiver_t* receiver, uint16_t transport_seq, uint32_t timestamp, size_t size, int remb);

/*更新网络的rtt*/
typedef void(*receiver_update_rtt_func)(razor_receiver_t* receiver, int32_t rtt);

/*设置最小码率*/
typedef void(*receiver_set_min_bitrate_func)(razor_receiver_t*receiver, uint32_t bitrate);

/*设置最大码率*/
typedef void(*receiver_set_max_bitrate_func)(razor_receiver_t*receiver, uint32_t bitrate);

struct __razor_receiver
{
	receiver_heartbeat_func			heartbeat;
	receiver_on_received_func		on_received;
	receiver_update_rtt_func		update_rtt;
	receiver_set_max_bitrate_func	set_max_bitrate;
	receiver_set_min_bitrate_func	set_min_bitrate;
};

razor_receiver_t*	razor_receiver_create(int min_bitrate, int max_bitrate, int packet_header_size, void* handler, send_feedback_func cb);
void				razor_receiver_destroy(razor_receiver_t* receiver);

#ifdef __cplusplus
}
#endif

#endif


