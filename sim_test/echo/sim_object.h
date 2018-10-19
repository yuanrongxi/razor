/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sim_object_h__
#define __sim_object_h__

#include <stdint.h>
#include <windows.h>

#include "video_device.h"
#include "record_thread.h"
#include "play_thread.h"

/*与windows界面的通信事件*/
#define WM_CONNECT_SUCC				(WM_USER + 10)
#define WM_CONNECT_FAILED			(WM_CONNECT_SUCC + 1)
#define WM_TIMEOUT					(WM_CONNECT_SUCC + 2)
#define WM_DISCONNECTED				(WM_CONNECT_SUCC + 3)
#define WM_NET_INTERRUPT			(WM_CONNECT_SUCC + 4)
#define WM_NET_RECOVER				(WM_CONNECT_SUCC + 5)
#define WM_CHANGE_BITRATE			(WM_CONNECT_SUCC + 6)
#define WM_STATE_INFO				(WM_CONNECT_SUCC + 7)
#define WM_START_PLAY				(WM_CONNECT_SUCC + 8)
#define WM_STOP_PLAY				(WM_CONNECT_SUCC + 9)
#define WM_FIR_NOTIFY				(WM_CONNECT_SUCC + 10)


class SimFramework
{
public:
	SimFramework(HWND hwnd);
	~SimFramework();

public:
	void on_notify(int type, uint32_t val);
	void on_change_bitrate(uint32_t bitrate_kbps);
	void on_state(const char* info);

public:
	void init(uint16_t port, uint32_t conf_min_bitrate, uint32_t conf_start_bitrate, uint32_t conf_max_bitrate);
	void destroy();

	int connect(int transport_type, int padding, uint32_t user_id, const char* receiver_ip, uint16_t receiver_port);
	void disconnect();

	void start_recorder(CFVideoRecorder* rec);
	void stop_recorder();

	void start_player(uint32_t uid, CFVideoPlayer* play);
	void stop_player();

private:
	int					state_;
	HWND				hwnd_;			/*接收消息的UI窗口句柄*/
	VideoRecordhread	rec_;
	VideoPlayhread		play_;
};



#endif 
