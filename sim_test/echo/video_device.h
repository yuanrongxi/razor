/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __video_device_h_
#define __video_device_h_

#include <atlcomcli.h>

#include <string>
#include <vector>

#include "echo_h264_encoder.h"
#include "echo_h264_decoder.h"

#include "streams.h"
#include "qedit.h"
#include "DShowCameraPlay.h"
#include "DShowGrabberCB.h"

#include "dib.h"
#include "simple_lock.h"

using namespace ds;
using namespace std;

enum PIX_FORMAT
{
	YUV422 = 1,
	RGB24 = 2
};

#define WM_ENCODER_RESOLUTION	WM_USER + 100
#define WM_DECODER_RESOLUTION	WM_ENCODER_RESOLUTION + 1

#define MAX_PIC_SIZE 1024000

typedef struct{
	int				rate;			/*每秒的帧数*/
	PIX_FORMAT		pix_format;		/*视频输入源格式*/
	uint32_t		width;			/*输入视频的宽度*/
	uint32_t		height;			/*输入视频的高度*/
	uint32_t		codec_width;	/*编码视频的宽度*/
	uint32_t		codec_height;	/*编码视频的高度*/
}video_info_t;

class CFVideoRecorder
{
public:
	CFVideoRecorder(const std::wstring& dev_name);
	virtual ~CFVideoRecorder();

	bool			open();
	void			close();

	video_info_t&	get_video_info() { return info_; };
	void			set_video_info(video_info_t& info){ info_ = info; };

	void			set_view_hwnd(HWND hwnd, const RECT& rect);

	int				read(void* data, uint32_t size, int& key_frame, uint8_t& payload_type);

	void			on_change_bitrate(uint32_t bitrate_kbps);

	void			enable_encode();
	void			disable_encode();

	std::string		get_resolution();

private:
	bool			get_device_info();

	bool			capture_sample();
	void			rotate_pic();

private:
	bool				open_;

	std::wstring		dev_;
	video_info_t		info_;
	std::string			resolution_;

	//视频抓捕的图形器
	CameraPlay_Graph	cam_graph_;
	GUID				media_type_;

	CDib				dib_;

	int64_t			    frame_intval_; //帧间隔，毫秒单位
	LARGE_INTEGER		prev_timer_;
	LARGE_INTEGER       counter_frequency_;

	HWND				hwnd_;
	HDC					hwnd_hdc_;
	RECT                hwnd_rect_;

	uint8_t*			video_data_;
	uint32_t			video_data_size_;

	SimpleLock			lock_;

	/*增加编码器对象*/
	H264Encoder*		encoder_;
	bool				encode_on_;
};

class CFVideoPlayer 
{
public:
	CFVideoPlayer(HWND hWnd, const RECT& rect);
	virtual ~CFVideoPlayer();

	bool open();
	void close();

	int write(const void* data, uint32_t size, uint8_t payload_type);
	std::string get_resolution();
private:
	bool		open_;
	HWND		hwnd_;

	CDib		dib_;
	RECT        rect_;
	HDC			hdc_;

	SimpleLock	lock_;

	uint32_t    decode_data_width_;
	uint32_t    decode_data_height_;

	uint8_t*	data_;
	/*增加解码器对象*/
	H264Decoder* decoder_;

	std::string	resolution_;
};

int get_camera_input_devices(vector<std::wstring>& vec_cameras);

#endif


