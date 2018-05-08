/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "echo_h264_decoder.h"

#define BUFF_SIZE (1024 * 1024)
H264Decoder::H264Decoder()
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
}

H264Decoder::~H264Decoder()
{
	if (buff_ != NULL){
		free(buff_);
		buff_ = NULL;
	}

	if (inited_)
		destroy();
}

bool H264Decoder::init()
{
	if (inited_)
		return false;

	avcodec_register_all();

	de_codec_ = avcodec_find_decoder(CODEC_ID_H264);
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

void H264Decoder::destroy()
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

bool H264Decoder::decode(uint8_t *in, int in_size, uint8_t **out, int &out_width, int& out_height, int32_t &pict_type)
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


