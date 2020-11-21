/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "video_device.h"

#include <initguid.h>
#include <uuids.h>
#include <vector>
#include <dvdmedia.h>
#include <sstream>
#include <thread>
#include <chrono>

#include "echo_h264_encoder.h"
#include "echo_h264_decoder.h"

#include "echo_h265_encoder.h"
#include "echo_h265_decoder.h"

#include "gettimeofday.h"

using namespace std;
using namespace std::chrono;

CFVideoRecorder::CFVideoRecorder(const std::wstring& dev_name) : frame_intval_(100),
	video_data_(NULL),
	video_data_size_(0),
	hwnd_(NULL),
	hwnd_hdc_(NULL),
	dev_(dev_name)
{
	//捕捉分辨率
	info_.width = 320;
	info_.height = 240;
	//编码时的分辨率，也就是最终网络上传输的分辨率
	info_.codec_width = 160;
	info_.codec_height = 120;

	info_.codec = codec_h264;
	info_.rate = 8;
	info_.pix_format = RGB24;

	encoder_ = NULL;
	open_ = false;
	intra_frame_ = false;
	encode_on_ = false;
}

CFVideoRecorder::~CFVideoRecorder()
{
	if (open_){
		this->close();
	}
}

void CFVideoRecorder::set_view_hwnd(HWND hwnd, const RECT& rect)
{
	AutoSpLock auto_lock(lock_);
	if (open_){
		if (hwnd_hdc_ != NULL && hwnd_ != NULL)
			::ReleaseDC(hwnd_, hwnd_hdc_);

		hwnd_ = hwnd;
		hwnd_rect_ = rect;
		hwnd_hdc_ = ::GetDC(hwnd_);
	}
	else{
		hwnd_ = hwnd;
		hwnd_rect_ = rect;
	}
}

bool CFVideoRecorder::open()
{
	if (open_)
		return true;

	AutoSpLock auto_lock(lock_);

	/*计算采集间隔*/
	frame_intval_ = 1000 / info_.rate;

	//检查视频设备
	vector<SCaptureDevice> cap_devs;
	DS_Utils::EnumCaptureDevice(cap_devs);
	if (cap_devs.size() <= 0)
		return false;

	bool device_found = false;
	CComPtr<IBaseFilter> filter_cam;
	for (uint32_t i = 0; i < cap_devs.size(); i++){
		std::wstring devName = cap_devs[i].FriendlyName; //转成UTF8比较
		if (devName == dev_){
			filter_cam = cap_devs[i].pFilter;
			device_found = true;
			break;
		}
	}

	if (!device_found)
		return false;

	if (filter_cam == NULL)
		return false;

	HRESULT hr = cam_graph_.InitInstance(filter_cam, MEDIASUBTYPE_RGB24, CLSID_NullRenderer, info_.width, info_.height);
	if (FAILED(hr))
		return false;

	//获取视频驱动参数
	if (!get_device_info())
		return false;

	//启动视频图形器
	hr = cam_graph_.RunGraph();
	if (FAILED(hr))
		return false;
	else{
		open_ = true;

		// Get the local window render hdc
		if (hwnd_ != NULL)
			hwnd_hdc_ = ::GetDC(hwnd_);
		// Init counter frequency
		QueryPerformanceFrequency(&counter_frequency_);
		// Init prev_timer_
		QueryPerformanceCounter(&prev_timer_);


		dib_.create(info_.width, info_.height, 24);
	}

	//分配一个视频数据缓冲区
	video_data_size_ = info_.width * info_.height * 3;
	video_data_ = new uint8_t[video_data_size_];

	/*初始化编码器*/
	if (info_.codec == codec_h264)
		encoder_ = new H264Encoder();
	else
		encoder_ = new H265Encoder();

	if (!encoder_->init(info_.rate, info_.width, info_.height, info_.codec_width, info_.codec_height))
		return false;

	char tmp[64] = { 0 };
	sprintf(tmp, "%dx%d", info_.codec_width, info_.codec_height);
	resolution_ = tmp;

	encode_on_ = true;

	return true;
}

void CFVideoRecorder::close()
{
	AutoSpLock auto_lock(lock_);
	if (open_){
		HRESULT hr = cam_graph_.StopGraph();

		if (video_data_ != NULL){
			delete[]video_data_;
			video_data_ = NULL;
			video_data_size_ = 0;
		}

		if (hwnd_hdc_ != NULL){
			::ReleaseDC(hwnd_, hwnd_hdc_);
			hwnd_hdc_ = NULL;
		}

		dib_.destroy();

		open_ = false;
	}
	
	if (encoder_){
		encoder_->destroy();
		delete encoder_;
		encoder_ = NULL;
	}
}

void CFVideoRecorder::rotate_pic()
{
	unsigned char* data = (unsigned char*)dib_.get_dib_bits();
	unsigned char* src = NULL;
	unsigned char* dst = NULL;

	unsigned int line_num = info_.width * 3;
	for (int i = info_.height - 1; i >= 0; i--){
		src = (unsigned char *)data + i * line_num;
		dst = video_data_ + (info_.height - i - 1) * line_num;
		memcpy(dst, src, line_num);
	}

}

bool CFVideoRecorder::capture_sample()
{
	bool ret = false;

	CComPtr<ISampleGrabber> grabber = cam_graph_.GetPictureGrabber();
	if (grabber == NULL)
		return ret;

	for (;;){
		if (!open_)
			return ret;

		unsigned long data_size = video_data_size_;
		//获取一张图片
		HRESULT hr = grabber->GetCurrentBuffer((long *)&data_size, (long *)dib_.get_dib_bits());
		if (FAILED(hr))
			Sleep(1);
		else
			break;
	}

	rotate_pic();

	ret = true;

	return ret;
}

int CFVideoRecorder::read(void* data, uint32_t data_size, int& key_frame, uint8_t& payload_type)
{
	AutoSpLock auto_lock(lock_);

	int ret = 0, out_size = 0;

	if (!open_)
		return ret;

	key_frame = 0;

	static time_point<system_clock> begin = steady_clock::now();

	LARGE_INTEGER cur_timer;
	QueryPerformanceCounter(&cur_timer);

	LARGE_INTEGER elapsed_million_seconds;
	elapsed_million_seconds.QuadPart = cur_timer.QuadPart - prev_timer_.QuadPart;
	elapsed_million_seconds.QuadPart *= 1000;
	elapsed_million_seconds.QuadPart /= counter_frequency_.QuadPart;

	if (frame_intval_ > elapsed_million_seconds.QuadPart)
		return ret;

	uint64_t dur = duration_cast<milliseconds>(steady_clock::now() - begin).count();
	begin = steady_clock::now();
	prev_timer_ = cur_timer;

	data_size = 0;
	if (capture_sample()){
		// 如果local_hwnd_hdc_不为空，则需要在本地预览
		if (hwnd_hdc_ != NULL){
			/*dib_.set_dib_bits(video_data_, video_data_size_);*/
			dib_.stretch_blt(hwnd_hdc_, hwnd_rect_.left, hwnd_rect_.top, hwnd_rect_.right - hwnd_rect_.left,
				hwnd_rect_.bottom - hwnd_rect_.top, 0, 0, info_.width, info_.height);
		}

		if (encode_on_){
			/*编码，确定编码数据和是否是关键帧，如果是关键帧，key_frame = 1,否者 = 0*/
			if (encoder_->encode(video_data_, video_data_size_, (PixelFormat)info_.pix_format, (uint8_t*)data, &out_size, &key_frame, intra_frame_) && out_size > 0){
				key_frame = (key_frame == 0x0001 ? 1 : 0);
				data_size = out_size;
				payload_type = encoder_->get_payload_type();

				char tmp[64] = { 0 };
				info_.codec_width = encoder_->get_codec_width();
				info_.codec_height = encoder_->get_codec_height();
				sprintf(tmp, "%dx%d", info_.codec_width, info_.codec_height);
				resolution_ = tmp;

				intra_frame_ = false;
			}
		}
	}


	return data_size;
}

void CFVideoRecorder::on_change_bitrate(uint32_t bitrate_kbps, int lost)
{
	AutoSpLock auto_lock(lock_);

	if (encoder_ != NULL){
		encoder_->set_bitrate(bitrate_kbps, lost);
	}
}

void CFVideoRecorder::enable_encode()
{
	AutoSpLock auto_lock(lock_);
	encode_on_ = true;
}

void CFVideoRecorder::disable_encode()
{
	AutoSpLock auto_lock(lock_);
	encode_on_ = false;
}

void CFVideoRecorder::set_intra_frame()
{
	AutoSpLock auto_lock(lock_);
	intra_frame_ = true;
}

std::string	CFVideoRecorder::get_resolution()
{
	AutoSpLock auto_lock(lock_);
	return resolution_;
}

bool CFVideoRecorder::get_device_info()
{
	HRESULT hr;
	AM_MEDIA_TYPE mt;

	CComPtr<ISampleGrabber> grabber = cam_graph_.GetPictureGrabber();
	if (grabber == NULL)
		return false;

	hr = grabber->SetBufferSamples(TRUE);
	if (FAILED(hr))
		return false;

	hr = grabber->GetConnectedMediaType(&mt);
	if (FAILED(hr))
		return false;

	if (mt.majortype != MEDIATYPE_Video)
		return false;

	//m_nVideoMediaType = mt.subtype;

	long actual_width = 0, actual_height = 0;

	if (mt.formattype == FORMAT_MPEGVideo){
		VIDEOINFOHEADER *pInfo = (VIDEOINFOHEADER *)mt.pbFormat;
		actual_width = pInfo->bmiHeader.biWidth;
		actual_height = pInfo->bmiHeader.biHeight;

	}
	else if (mt.formattype == FORMAT_MPEG2Video){
		VIDEOINFOHEADER2 *pInfo = (VIDEOINFOHEADER2 *)mt.pbFormat;
		actual_width = pInfo->bmiHeader.biWidth;
		actual_height = pInfo->bmiHeader.biHeight;

	}
	else if (mt.formattype == FORMAT_VideoInfo){
		VIDEOINFOHEADER *pInfo = (VIDEOINFOHEADER *)mt.pbFormat;
		actual_width = pInfo->bmiHeader.biWidth;
		actual_height = pInfo->bmiHeader.biHeight;

	}
	else if (mt.formattype == FORMAT_VideoInfo2){
		VIDEOINFOHEADER2 *pInfo = (VIDEOINFOHEADER2 *)mt.pbFormat;
		actual_width = pInfo->bmiHeader.biWidth;
		actual_height = pInfo->bmiHeader.biHeight;
	}
	else
		return false;

	if (info_.width != actual_width || info_.height != actual_height)
		return false;

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
CFVideoPlayer::CFVideoPlayer(HWND hWnd, const RECT& rc)
{
	hwnd_ = hWnd;
	rect_ = rc;
	open_ = false;

	//data_ = (uint8_t *)malloc(sizeof(uint8_t) * MAX_PIC_SIZE);
	resolution_ = "";
}

CFVideoPlayer::~CFVideoPlayer()
{
	this->close();
}

bool CFVideoPlayer::open()
{
	if (open_)
		return true;

	if (hwnd_ != NULL){
		hdc_ = ::GetDC(hwnd_);
		if (hdc_ == NULL)
			return false;
	}

	codec_type_ = codec_h264;
	decoder_ = new H264Decoder();
	if (!decoder_->init()){
		::ReleaseDC(hwnd_, hdc_);
		hdc_ = NULL;
	}

	open_ = true;

	return true;
}

void CFVideoPlayer::close()
{
	if (open_){
		lock_.Lock();

		if (hdc_ != NULL && hwnd_ != NULL){
			::ReleaseDC(hwnd_, hdc_);
			hdc_ = NULL;
		}

		lock_.Unlock();

		open_ = false;

		if (decoder_ != NULL){
			decoder_->destroy();
			delete decoder_;
			decoder_ = NULL;
		}
	}
}

#define SU_MAX(a, b) ((a) > (b) ? (a) : (b))

int CFVideoPlayer::write(const void* data, uint32_t size, uint8_t payload_type)
{
	int width, height;

	int32_t pic_type;

	if (size > 0){

		if (payload_type != codec_type_){
			decoder_->destroy();
			delete decoder_;

			codec_type_ = payload_type;
			if (payload_type == codec_h264){
				decoder_ = new H264Decoder();
			}
			else{
				decoder_ = new H265Decoder();
			}

			decoder_->init();
		}

		/*先解码，在显示*/
		if(decoder_ != NULL && !decoder_->decode((uint8_t *)data, size, &data_, width, height, pic_type))
			return 0;

		if (decode_data_width_ != width || decode_data_height_ != height){
			// When first time write or the width or height changed
			decode_data_width_ = width;
			decode_data_height_ = height;

			if (dib_.is_valid())
				dib_.destroy();

			dib_.create(decode_data_width_, decode_data_height_);

			char tmp[64] = { 0 };
			sprintf(tmp, "%dx%d", decode_data_width_, decode_data_height_);
			resolution_ = tmp;
		}

		dib_.set_dib_bits((void *)data_, decode_data_width_ * decode_data_height_ * 3, false);

		lock_.Lock();

		if (hdc_ != NULL){
			dib_.stretch_blt(hdc_, 0, 0, rect_.right - rect_.left, rect_.bottom - rect_.top,
				0, 0, decode_data_width_, decode_data_height_);
		}

		lock_.Unlock();
	}

	return 0;
}

std::string	CFVideoPlayer::get_resolution()
{
	AutoSpLock auto_lock(lock_);
	return resolution_;
}

/////////////////////////////////////////////////////////////////////////////

int get_camera_input_devices(vector<std::wstring>& vec_cameras)
{
	vector<SCaptureDevice> cap_devs;
	DS_Utils::EnumCaptureDevice(cap_devs);
	if (cap_devs.size() == 0)
		return 0;

	for (uint32_t i = 0; i < cap_devs.size(); ++i)
		vec_cameras.push_back(cap_devs[i].FriendlyName);

	return vec_cameras.size();
}



