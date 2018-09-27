/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __razor_api__001_h_
#define __razor_api__001_h_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************
说明:razor没有实现线程安全，多线程访问需要上层的通信模块来保证，我们
是这样考虑的，因为整个拥塞评估没有很大的计算，也没有并发的需求，只要
在通信层做线程安全即可，关于这个请看具体的sim transport使用的例子
**************************************************************/

/*定义回调函数和对象*/
#include "razor_callback.h"

enum
{
	gcc_congestion = 0,
	bbr_congestion = 1,
};

/****************************************外部直接调用API************************************/
void				razor_setup_log(razor_log_func log_cb);
/*创建一个发送端的拥塞控制对象*/
razor_sender_t*		razor_sender_create(int type, int padding, void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms);
/*销毁一个发送端的拥塞控制对象*/
void				razor_sender_destroy(razor_sender_t* sender);

razor_receiver_t*	razor_receiver_create(int type, int min_bitrate, int max_bitrate, int packet_header_size, void* handler, send_feedback_func cb);
void				razor_receiver_destroy(razor_receiver_t* receiver);

#ifdef __cplusplus
}
#endif

#endif


