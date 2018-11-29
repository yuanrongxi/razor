/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "codec_common.h"
#include <assert.h>

const encoder_resolution_t h264_resolution_infos[RESOLUTIONS_NUMBER] = {
	{ VIDEO_120P, PIC_WIDTH_160, PIC_HEIGHT_120, 48, 32, 48 },
	{ VIDEO_240P, PIC_WIDTH_320, PIC_HEIGHT_240, 140, 80, 120 },
	{ VIDEO_360P, PIC_WIDTH_480, PIC_HEIGHT_360, 300, 180, 240 },
	{ VIDEO_480P, PIC_WIDTH_640, PIC_HEIGHT_480, 880, 360, 800 },
	{ VIDEO_640P, PIC_WIDTH_960, PIC_HEIGHT_640, 1480, 1000, 1200 },
	{ VIDEO_720P, PIC_WIDTH_1280, PIC_HEIGHT_720, 2200, 1600, 1800 },
	{ VIDEO_1080P, PIC_WIDTH_1920, PIC_HEIGHT_1080, 4000, 2400, 2800 },
};

const encoder_resolution_t h265_resolution_infos[RESOLUTIONS_NUMBER] = {
	{ VIDEO_120P, PIC_WIDTH_160, PIC_HEIGHT_120, 40, 24, 32 },
	{ VIDEO_240P, PIC_WIDTH_320, PIC_HEIGHT_240, 120, 72, 96 },
	{ VIDEO_360P, PIC_WIDTH_480, PIC_HEIGHT_360, 240, 144, 200 },
	{ VIDEO_480P, PIC_WIDTH_640, PIC_HEIGHT_480, 720, 280, 640 },
	{ VIDEO_640P, PIC_WIDTH_960, PIC_HEIGHT_640, 1000, 800, 900 },
	{ VIDEO_720P, PIC_WIDTH_1280, PIC_HEIGHT_720, 1800, 1440, 1600 },
	{ VIDEO_1080P, PIC_WIDTH_1920, PIC_HEIGHT_1080, 3200, 2000, 2200 },
};

encoder_resolution_t resolution_infos[RESOLUTIONS_NUMBER];

/*load ffmpeg lib*/
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "libx264.dll.a")
#ifdef _DEBUG
#pragma comment(lib, "x265-staticd.lib")
#else
#pragma comment(lib, "x265-static.lib")
#endif

extern int frame_log(int level, const char* file, int line, const char *fmt, ...);

void setup_codec(int codec_id)
{
	switch (codec_id){
		case codec_h264:
			for (int i = 0; i < RESOLUTIONS_NUMBER; ++i)
				resolution_infos[i] = h264_resolution_infos[i];
			break;

		case codec_h265:
			for (int i = 0; i < RESOLUTIONS_NUMBER; ++i)
				resolution_infos[i] = h265_resolution_infos[i];
			break;

		default:
			assert(0);
	}
}

/***********************************************************************************************************/
VideoEncoder::VideoEncoder()
{
	inited_ = false;

	src_width_ = PIC_WIDTH_640;
	src_height_ = PIC_HEIGHT_480;

	frame_rate_ = DEFAULT_FRAME_RATE;
	max_resolution_ = VIDEO_480P;
	curr_resolution_ = VIDEO_480P;

	bitrate_kbps_ = 800000;

	inited_ = false;
	frame_index_ = 0;
}

VideoEncoder::~VideoEncoder()
{
	if (inited_)
		destroy();
}

bool VideoEncoder::init(int frame_rate, int src_width, int src_height, int dst_width, int dst_height)
{
	if (inited_)
		return false;

	src_width_ = src_width;
	src_height_ = src_height;

	frame_rate_ = frame_rate;

	int i;
	for (i = VIDEO_1080P; i > VIDEO_120P; --i){
		if (dst_width == resolution_infos[i].codec_width && dst_height == resolution_infos[i].codec_height)
			break;
	}

	max_resolution_ = i;
	curr_resolution_ = max_resolution_;

	bitrate_kbps_ = (resolution_infos[curr_resolution_].max_rate + resolution_infos[curr_resolution_].min_rate) / 2;

	if (!open_encoder())
		return false;

	inited_ = true;

	rate_stat_init(&rate_stat_, KEY_FRAME_SEC * 1000, 8000);
	return true;
}

void VideoEncoder::destroy()
{
	if (!inited_)
		return;

	inited_ = false;
	close_encoder();

	max_resolution_ = VIDEO_480P;
	curr_resolution_ = VIDEO_480P;

	rate_stat_destroy(&rate_stat_);
}

void VideoEncoder::set_bitrate(uint32_t bitrate_kbps, int lost)
{
	int index = (lost == 1 ? curr_resolution_ : max_resolution_);
	bitrate_kbps_ = bitrate_kbps;

	if (bitrate_kbps > resolution_infos[index].max_rate)
		bitrate_kbps = resolution_infos[index].max_rate;

	reconfig_encoder(bitrate_kbps);
}

int VideoEncoder::get_codec_width() const
{
	return resolution_infos[curr_resolution_].codec_width;;
}

int VideoEncoder::get_codec_height() const
{
	return resolution_infos[curr_resolution_].codec_height;
}

int VideoEncoder::get_payload_type() const
{
	return payload_type_;
}

int VideoEncoder::find_resolution(uint32_t birate_kpbs)
{
	int ret;
	for (ret = max_resolution_; ret > VIDEO_120P; --ret){
		if (resolution_infos[ret].max_rate >= birate_kpbs && resolution_infos[ret - 1].max_rate < birate_kpbs)
			break;
	}

	frame_log(0, __FILE__, __LINE__, "resolution = %u, birate_kpbs = %u\n", ret, birate_kpbs);

	return ret;
}

void VideoEncoder::try_change_resolution()
{
	/*判断下一帧处在gop的位置, 如果处于后半段，我们可以尝试改变分辨率*/
	uint32_t frame_index = (frame_index_ + 1) % (frame_rate_ * KEY_FRAME_SEC);
	const encoder_resolution_t& res = resolution_infos[curr_resolution_];
	if (res.min_rate > bitrate_kbps_ && curr_resolution_ > VIDEO_120P){
		/*降低一层分辨率*/
		int32_t rate_stat_kps = rate_stat_rate(&rate_stat_, GET_SYS_MS()) / 1000;
		if (rate_stat_kps < res.min_rate * 7 / 8) /*产生数据的带宽小于最小限制带宽，不做改动*/
			return;

		curr_resolution_ = find_resolution(bitrate_kbps_);
		close_encoder();
		open_encoder();
		set_bitrate(bitrate_kbps_, 0);

		rate_stat_reset(&rate_stat_);
	}
	else if (frame_index >= (KEY_FRAME_SEC * frame_rate_ / 2) && res.max_rate < bitrate_kbps_ && curr_resolution_ + 1 <= max_resolution_){
		/*升高一层分辨率*/
		curr_resolution_ = find_resolution(bitrate_kbps_);
		close_encoder();
		open_encoder();
		set_bitrate(bitrate_kbps_, 0);

		rate_stat_reset(&rate_stat_);
	}
}

/***********************************************************************************************************/
#define BUFF_SIZE (1024 * 1024)
VideoDecoder::VideoDecoder()
{
	inited_ = false;
	curr_width_ = 0;
	curr_height_ = 0;

	buff_ = (uint8_t*)malloc(BUFF_SIZE);
	buff_size_ = BUFF_SIZE;

	de_frame_ = NULL;
	de_codec_ = NULL;

	de_context_ = NULL;
	sws_context_ = NULL;

	codec_id_ = CODEC_ID_H264;
}

VideoDecoder::~VideoDecoder()
{
	if (inited_)
		destroy();

	if (buff_ != NULL){
		free(buff_);
		buff_ = NULL;
	}
}

bool VideoDecoder::init()
{
	if (inited_)
		return false;

	avcodec_register_all();

	set_codec_id();

	de_codec_ = avcodec_find_decoder(codec_id_);
	de_context_ = avcodec_alloc_context3(de_codec_);

	de_context_->error_concealment = 1;
	de_frame_ = avcodec_alloc_frame();

	if (avcodec_open2(de_context_, de_codec_, NULL) != 0){
		if (de_context_ != NULL){
			avcodec_close(de_context_);
			av_free(de_context_);
			de_context_ = NULL;
		}

		if (de_frame_ != NULL){
			av_free(de_frame_);
			de_frame_ = NULL;
		}
		return false;
	}

	inited_ = true;

	return true;
}

void VideoDecoder::destroy()
{
	if (!inited_)
		return;

	inited_ = false;

	if (de_context_ != NULL){
		avcodec_close(de_context_);
		av_free(de_context_);
		de_context_ = NULL;
	}

	if (de_frame_ != NULL){
		av_free(de_frame_);
		de_frame_ = NULL;
	}

	if (sws_context_ != NULL){
		sws_freeContext(sws_context_);
		sws_context_ = NULL;
	}
}

bool VideoDecoder::decode(uint8_t *in, int in_size, uint8_t **out, int &out_width, int& out_height, int32_t &pict_type)
{
	bool ret = false;
	if (!inited_)
		return ret;

	av_init_packet(&avpkt_);
	avpkt_.data = in;
	avpkt_.size = in_size;

	int got_picture = 0;
	int out_size = avcodec_decode_video2(de_context_, de_frame_, &got_picture, &avpkt_);
	if (out_size <= 0)
		return ret;

	if (curr_width_ != de_frame_->width || curr_height_ != de_frame_->height){
		// When first time decoding or the width or height changed, the following code will execute
		curr_width_ = de_frame_->width;
		curr_height_ = de_frame_->height;

		if (curr_height_ * curr_width_ * 3 + 10 > buff_size_){
			buff_size_ = curr_height_ * curr_width_ * 3 + 10;
			buff_ = (uint8_t*)realloc(buff_, buff_size_);
		}

		if (sws_context_ != NULL)
			sws_freeContext(sws_context_);

		// No scale here, so use the same width and height
		sws_context_ = sws_getContext(curr_width_, curr_height_, PIX_FMT_YUV420P,
			curr_width_, curr_height_, PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}

	if (sws_context_ != NULL){
		avpicture_fill(&outpic_, buff_, PIX_FMT_RGB24, curr_width_, curr_height_);
		//YUV -> RGB, No scale
		out_size = sws_scale(sws_context_, de_frame_->data, de_frame_->linesize, 0,
			de_context_->height, outpic_.data, outpic_.linesize);
		if (out_size > 0){
			*out = buff_;
			out_width = curr_width_;
			out_height = curr_height_;
			pict_type = de_frame_->pict_type;
			ret = true;
		}
	}
	return ret;
}




