/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_object.h"
#include "sim_external.h"
#include "audio_log.h"

#include <time.h>
#include <assert.h>

static void notify_callback(void * event, int type, uint32_t val)
{
	SimFramework* frame = (SimFramework*)event;
	if (frame != NULL)
		frame->on_notify(type, val);
}

static void notify_change_bitrate(void * event, uint32_t bitrate_kbps)
{
	SimFramework* frame = (SimFramework*)event;
	if (frame != NULL)
		frame->on_change_bitrate(bitrate_kbps);
}

static void notify_state(void * event, const char* info)
{
	SimFramework* frame = (SimFramework*)event;
	if (frame != NULL)
		frame->on_state(info);
}

enum{
	eframe_idle,
	eframe_inited,
	eframe_connecting,
	eframe_connected,
	eframe_disconnect,
};

SimFramework::SimFramework(HWND hwnd) : state_(eframe_idle), hwnd_(hwnd)
{

}

SimFramework::~SimFramework()
{
	if (state_ != eframe_idle)
		destroy();
}

void SimFramework::init(uint16_t port, uint32_t conf_min_bitrate, uint32_t conf_start_bitrate, uint32_t conf_max_bitrate)
{
	if (state_ != eframe_idle)
		return;

	if (open_win_log("echo.log") != 0){
		assert(0);
		return;
	}

	/*初始化sim transport模拟传输模块*/
	sim_init(port, this, log_win_write, notify_callback, notify_change_bitrate, notify_state);
	sim_set_bitrates(conf_min_bitrate, conf_start_bitrate, conf_max_bitrate);

	state_ = eframe_inited;
}

void SimFramework::destroy()
{
	if (state_ == eframe_idle)
		return;

	this->disconnect();

	state_ = eframe_idle;

	close_win_log();

	sim_destroy();
}

int SimFramework::connect(int transport_type, uint32_t user_id, const char* receiver_ip, uint16_t receiver_port)
{
	if (state_ != eframe_inited)
		return -1;

	//连接接收端
	if (sim_connect(user_id, receiver_ip, receiver_port, transport_type) != 0){
		printf("sim connect failed!\n");
		return -2;
	}

	state_ = eframe_connecting;

	return 0;
}

void SimFramework::disconnect()
{
	//停止录制和播放线程
	stop_recorder();

	if (state_ == eframe_connecting || state_ == eframe_connected){
		sim_disconnect();

		int i = 0;
		state_ = eframe_disconnect;
		while (state_ == eframe_disconnect && i++ < 20)
			::Sleep(100);
	}

	state_ = eframe_inited;
}

void SimFramework::start_recorder(CFVideoRecorder* rec)
{
	if (state_ == eframe_connected){
		rec_.set_video_devices(rec);
		rec_.start();
	}
}

void  SimFramework::stop_recorder()
{
	rec_.stop();
}

void SimFramework::start_player(uint32_t uid, CFVideoPlayer* play)
{
	play_.set_video_devices(play);
	play_.start();
}

void SimFramework::stop_player()
{
	play_.stop();
}

void SimFramework::on_notify(int type, uint32_t val)
{
	if (hwnd_ == NULL)
		return;

	switch (type){
	case sim_connect_notify:
		if (val == 0){
			::PostMessage(hwnd_, WM_CONNECT_SUCC, NULL, NULL);
			if (state_ == eframe_connecting)
				state_ = eframe_connected;
		}
		else{
			::PostMessage(hwnd_, WM_CONNECT_FAILED, (WPARAM)val, NULL);
			state_ = eframe_inited;
		}

		break;

	case sim_network_timout:
		if (state_ > eframe_inited){
			::PostMessage(hwnd_, WM_TIMEOUT, (WPARAM)val, NULL);
			state_ = eframe_inited;
		}
		break;

	case sim_disconnect_notify:
		if (state_ == eframe_disconnect){
			::PostMessage(hwnd_, WM_DISCONNECTED, (WPARAM)val, NULL);
			state_ = eframe_inited;
		}
		break;

	case sim_start_play_notify:
		::PostMessage(hwnd_, WM_START_PLAY, (WPARAM)val, NULL);
		break;

	case sim_stop_play_notify:
		::PostMessage(hwnd_, WM_STOP_PLAY, (WPARAM)val, NULL);
		break;

	case net_interrupt_notify:
		if (state_ == eframe_connected)
			::PostMessage(hwnd_, WM_NET_INTERRUPT, (WPARAM)val, NULL);
		break;

	case net_recover_notify:
		if (state_ == eframe_connected)
			::PostMessage(hwnd_, WM_NET_RECOVER, (WPARAM)val, NULL);
		break;

	case sim_fir_notify:
		if (state_ == eframe_connected)
			::PostMessage(hwnd_, WM_FIR_NOTIFY, NULL, NULL);
		break;
	}
}

void SimFramework::on_change_bitrate(uint32_t bitrate_kbps)
{
	if (hwnd_ != NULL)
		::PostMessage(hwnd_, WM_CHANGE_BITRATE, (WPARAM)bitrate_kbps, NULL);
}

void SimFramework::on_state(const char* info)
{
	if (hwnd_ != NULL){
		char* buff;
		buff = (char*)malloc(strlen(info) + 10);
		strcpy(buff, info);

		::PostMessage(hwnd_, WM_STATE_INFO, (WPARAM)buff, NULL);
	}
}

