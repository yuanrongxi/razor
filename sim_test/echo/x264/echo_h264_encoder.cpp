/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "echo_h264_encoder.h"

/*load ffmpeg lib*/
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

H264Encoder::H264Encoder()
{
	src_width_ = PIC_WIDTH_640;
	src_height_ = PIC_HEIGHT_480;

	codec_width_ = PIC_WIDTH_640;
	codec_height_ = PIC_HEIGHT_480;

	sws_context_ = NULL;

	resolution_ = VIDEO_240P;
	frame_rate_ = DEFAULT_FRAME_RATE;
	/*240P码率范围，12.5KB/s ~ 20KB/s*/
	min_rate_ = 100;
	max_rate_ = 160;

	inited_ = false;

	en_h_ = NULL;

	memset(&en_picture_, 0, sizeof(en_picture_));
	memset(&en_param_, 0, sizeof(en_param_));
}

H264Encoder::~H264Encoder()
{
	if (inited_)
		destroy();
}

bool H264Encoder::init(int frame_rate, int src_width, int src_height, int dst_width, int dst_height)
{
	if (inited_)
		return false;

	src_width_ = src_width;
	src_height_ = src_height;

	codec_width_ = dst_width;
	codec_height_ = dst_height;

	frame_rate_ = frame_rate;

	/*确定支持的分辨率*/
	if (dst_width == PIC_WIDTH_1920 && dst_height == PIC_HEIGHT_1080)
		resolution_ = VIDEO_1080P;
	else if (dst_width == PIC_WIDTH_1280 && dst_height == PIC_HEIGHT_720)
		resolution_ = VIDEO_720P;
	else if (dst_width == PIC_WIDTH_960 && dst_height == PIC_HEIGHT_640)
		resolution_ = VIDEO_640P;
	else if (dst_width == PIC_WIDTH_640 && dst_height == PIC_HEIGHT_480)
		resolution_ = VIDEO_480P;
	else if (dst_width == PIC_WIDTH_480 && dst_height == PIC_HEIGHT_360)
		resolution_ = VIDEO_360P;
	else if (dst_width == PIC_WIDTH_320 && dst_height == PIC_HEIGHT_240)
		resolution_ = VIDEO_240P;
	else if (dst_width == PIC_WIDTH_160 && dst_height == PIC_HEIGHT_120)
		resolution_ = VIDEO_120P;
	else
		return false;

	x264_param_default(&en_param_);
	if (x264_param_default_preset(&en_param_, "faster", "zerolatency") != 0)//medium,veryslow
		return false;

	config_param();

	//构造编码器
	if (x264_param_apply_profile(&en_param_, "main") != 0)
		return false;

	if ((en_h_ = x264_encoder_open(&en_param_)) == NULL)
		return false;

	if (x264_picture_alloc(&en_picture_, X264_CSP_I420, codec_width_, codec_height_) != 0)
		return false;

	//构造颜色空间转换器
	sws_context_ = sws_getContext(src_width_, src_height_,
		PIX_FMT_BGR24, codec_width_, codec_height_,
		PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	inited_ = true;

	return true;
}

void H264Encoder::destroy()
{
	if (!inited_)
		return;

	if (en_h_ != NULL){
		x264_picture_clean(&en_picture_);
		x264_encoder_close(en_h_);
		en_h_ = NULL;

		sws_freeContext(sws_context_);
		sws_context_ = NULL;
	}

	resolution_ = VIDEO_240P;
}

void H264Encoder::set_bitrate(uint32_t bitrate_kbps)
{
	if (bitrate_kbps > max_rate_)
		bitrate_kbps = max_rate_;
	else if (bitrate_kbps < min_rate_)
		bitrate_kbps = min_rate_;

	if (bitrate_kbps != en_param_.rc.i_vbv_max_bitrate){
		en_param_.rc.i_vbv_max_bitrate = bitrate_kbps;
		en_param_.rc.i_bitrate = bitrate_kbps - bitrate_kbps / 4;

		/*从新配置x.264的码率,及时生效*/
		if (en_h_ != NULL)
			x264_encoder_reconfig(en_h_, &en_param_);
	}
}

uint32_t H264Encoder::get_bitrate() const
{
	return en_param_.rc.i_vbv_max_bitrate;
}

void H264Encoder::config_param()
{
	en_param_.i_threads = X264_THREADS_AUTO;
	en_param_.i_width = codec_width_;
	en_param_.i_height = codec_height_;

	en_param_.i_fps_num = frame_rate_;
	en_param_.i_fps_den = 1;
	en_param_.rc.i_qp_step = 2;

	en_param_.i_log_level = X264_LOG_NONE;
	en_param_.rc.i_rc_method = X264_RC_CRF;


	switch (resolution_){
	case VIDEO_1080P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 40;
		en_param_.rc.i_qp_constant = 24;
		en_param_.rc.i_bitrate = 1600;
		en_param_.rc.i_vbv_max_bitrate = 3200;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 0;

		max_rate_ = 3200;
		min_rate_ = 1600;
		break;
	case VIDEO_720P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 40;
		en_param_.rc.i_qp_constant = 24;
		en_param_.rc.i_bitrate = 1400;
		en_param_.rc.i_vbv_max_bitrate = 2000;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 0;

		max_rate_ = 2000;
		min_rate_ = 1400;
		break;

	case VIDEO_640P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 40;
		en_param_.rc.i_qp_constant = 24;
		en_param_.rc.i_bitrate = 640;
		en_param_.rc.i_vbv_max_bitrate = 800;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 0;

		max_rate_ = 800;
		min_rate_ = 640;
		break;

	case VIDEO_480P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 40;
		en_param_.rc.i_qp_constant = 28;
		en_param_.rc.i_bitrate = 320;
		en_param_.rc.i_vbv_max_bitrate = 640;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 0;

		max_rate_ = 1000;
		min_rate_ = 400;
		break;

	case VIDEO_360P:
		en_param_.rc.i_qp_min = 15;
		en_param_.rc.i_qp_max = 38;
		en_param_.rc.i_qp_constant = 24;
		en_param_.rc.i_bitrate = 240;
		en_param_.rc.i_vbv_max_bitrate = 320;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 0;

		max_rate_ = 320;
		min_rate_ = 240;
		break;

	case VIDEO_240P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 36;
		en_param_.rc.i_qp_constant = 28;
		en_param_.rc.i_bitrate = 100;
		en_param_.rc.i_vbv_max_bitrate = 160;
		en_param_.i_keyint_min = frame_rate_ * 5;
		en_param_.i_keyint_max = frame_rate_ * 5;
		en_param_.i_bframe = 0;

		max_rate_ = 160;
		min_rate_ = 100;
		break;

	case VIDEO_120P:
		en_param_.rc.i_qp_min = 5;
		en_param_.rc.i_qp_max = 32;
		en_param_.rc.i_qp_constant = 24;
		en_param_.rc.i_bitrate = 32;
		en_param_.rc.i_vbv_max_bitrate = 40;
		en_param_.i_keyint_min = frame_rate_ * 6;
		en_param_.i_keyint_max = frame_rate_ * 6;
		en_param_.i_bframe = 1;

		max_rate_ = 40;
		min_rate_ = 32;
		break;
	}
	 
	en_param_.rc.f_rate_tolerance = 1.0;
	en_param_.rc.i_vbv_buffer_size = 800;
	en_param_.rc.f_vbv_buffer_init = 0.9f;

	en_param_.rc.b_mb_tree = 1;
	en_param_.b_repeat_headers = 1;
	en_param_.b_annexb = 1;
	en_param_.b_intra_refresh = 0;

	en_param_.i_frame_reference = 4;
	en_param_.vui.b_fullrange = 1;
	en_param_.i_sync_lookahead = 0;
	en_param_.vui.i_colorprim = 2;
	en_param_.vui.i_transfer = 2;
	en_param_.vui.i_colmatrix = 2;
	//en_param_.vui.i_chroma_loc = 2;
	en_param_.i_cabac_init_idc = 2;
	en_param_.rc.i_aq_mode = X264_AQ_VARIANCE;
	en_param_.rc.f_aq_strength = 1.3f;
	en_param_.rc.f_qcompress = 0.75;

	en_param_.b_deblocking_filter = 0x00000800;
	en_param_.i_deblocking_filter_alphac0 = 2;
	en_param_.i_deblocking_filter_beta = 2;
	en_param_.analyse.i_me_method = X264_ME_DIA;
	en_param_.analyse.inter = X264_ANALYSE_I8x8 | X264_ANALYSE_I4x4;
}

bool H264Encoder::encode(uint8_t *in, int in_size, enum PixelFormat src_pix_fmt, uint8_t *out, int *out_size, int *frame_type)
{
	if (!inited_)
		return false;

	int ret;
	static AVPicture pic = { 0 };

	en_param_.i_frame_total++;

	//RGB -> YUV
	if (src_pix_fmt == PIX_FMT_RGB24 || src_pix_fmt == PIX_FMT_BGR24){
		int src_stride = src_width_ * 3;
		ret = sws_scale(sws_context_, &in, &src_stride, 0, src_height_,
			en_picture_.img.plane, en_picture_.img.i_stride);
	}

	en_picture_.i_pts = (int64_t)en_param_.i_frame_total * en_param_.i_fps_den;

	x264_nal_t *nal = NULL;
	int i_nal = 0;

	//X.264 编码
	ret = x264_encoder_encode(en_h_, &nal, &i_nal, &en_picture_, &pic_out_);
	if (ret > 0){
		*out_size = ret;
		memcpy(out, nal[0].p_payload, ret);

		*frame_type = pic_out_.i_type;
		return true;
	}

	return false;
}
