/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "echo_h264_encoder.h"

extern int frame_log(int level, const char* file, int line, const char *fmt, ...);

H264Encoder::H264Encoder()
{
	src_width_ = PIC_WIDTH_640;
	src_height_ = PIC_HEIGHT_480;

	sws_context_ = NULL;

	frame_rate_ = DEFAULT_FRAME_RATE;
	max_resolution_ = VIDEO_480P;
	curr_resolution_ = VIDEO_480P;

	bitrate_kbps_ = 800000;

	inited_ = false;
	frame_index_ = 0;

	en_h_ = NULL;

	memset(&en_picture_, 0, sizeof(en_picture_));
	memset(&en_param_, 0, sizeof(en_param_));
}

H264Encoder::~H264Encoder()
{
	if (inited_)
		destroy();
}


bool H264Encoder::open_encoder()
{
	x264_param_default(&en_param_);
	if (x264_param_default_preset(&en_param_, "faster", "zerolatency") != 0)//medium,veryslow
		return false;

	config_param();

	//构造编码器
	if (x264_param_apply_profile(&en_param_, "main") != 0)
		return false;

	if ((en_h_ = x264_encoder_open(&en_param_)) == NULL)
		return false;

	if (x264_picture_alloc(&en_picture_, X264_CSP_I420, resolution_infos[curr_resolution_].codec_width, resolution_infos[curr_resolution_].codec_height) != 0)
		return false;

	//构造颜色空间转换器
	sws_context_ = sws_getContext(src_width_, src_height_,
		PIX_FMT_BGR24, resolution_infos[curr_resolution_].codec_width, resolution_infos[curr_resolution_].codec_height,
		PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	frame_index_ = 0;

	return true;
}

void H264Encoder::close_encoder()
{
	if (en_h_ != NULL){
		x264_picture_clean(&en_picture_);
		x264_encoder_close(en_h_);
		en_h_ = NULL;

		sws_freeContext(sws_context_);
		sws_context_ = NULL;
	}

	frame_index_ = 0;
}

void H264Encoder::reconfig_encoder(uint32_t bitrate_kbps)
{
	bitrate_kbps_ = bitrate_kbps;

	en_param_.rc.i_vbv_max_bitrate = bitrate_kbps;
	en_param_.rc.i_bitrate = (bitrate_kbps + resolution_infos[curr_resolution_].min_rate) / 2;
	if (bitrate_kbps != en_param_.rc.i_vbv_max_bitrate){
		if (en_param_.rc.i_bitrate < resolution_infos[curr_resolution_].min_rate)
			bitrate_kbps = resolution_infos[curr_resolution_].min_rate;

		/*从新配置x.264的码率,及时生效*/
		if (en_h_ != NULL)
			x264_encoder_reconfig(en_h_, &en_param_);
	}
}

int H264Encoder::get_bitrate() const
{
	return en_param_.rc.i_vbv_max_bitrate;
}

int H264Encoder::get_codec_width() const
{
	return resolution_infos[curr_resolution_].codec_width;
}

int H264Encoder::get_codec_height() const
{
	return resolution_infos[curr_resolution_].codec_height;
}

void H264Encoder::config_param()
{
	const encoder_resolution_t& res = resolution_infos[curr_resolution_];
	en_param_.i_threads = X264_THREADS_AUTO;
	en_param_.i_width = res.codec_width;
	en_param_.i_height = res.codec_height;

	en_param_.i_fps_num = frame_rate_;
	en_param_.i_fps_den = 1;
	en_param_.rc.i_qp_step = 2;

	en_param_.i_log_level = X264_LOG_NONE;
	en_param_.rc.i_rc_method = X264_RC_CRF;

	en_param_.rc.i_qp_min = 0;
	en_param_.rc.i_qp_max = 40;
	en_param_.rc.i_qp_constant = 8;
	en_param_.rc.i_bitrate = (res.min_rate + res.max_rate) / 2;
	en_param_.rc.i_vbv_max_bitrate = res.max_rate;
	en_param_.i_bframe = 0;

	 
	en_param_.i_keyint_min = frame_rate_ * KEY_FRAME_SEC;
	en_param_.i_keyint_max = frame_rate_ * KEY_FRAME_SEC;

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

bool H264Encoder::encode(uint8_t *in, int in_size, enum PixelFormat src_pix_fmt, uint8_t *out, int *out_size, int *frame_type, bool request_keyframe)
{
	bool ret = false;
	if (!inited_)
		return ret;

	int rc;

	try_change_resolution();

	en_param_.i_frame_total++;
	frame_index_++;

	//RGB -> YUV
	if (src_pix_fmt == PIX_FMT_RGB24 || src_pix_fmt == PIX_FMT_BGR24){
		int src_stride = src_width_ * 3;
		rc = sws_scale(sws_context_, &in, &src_stride, 0, src_height_,
			en_picture_.img.plane, en_picture_.img.i_stride);
	}

	en_picture_.i_pts = (int64_t)en_param_.i_frame_total * en_param_.i_fps_den;

	x264_nal_t *nal = NULL;
	int i_nal = 0;

	/*指定编码输出关键帧*/
	en_picture_.i_type = request_keyframe ? X264_TYPE_IDR : X264_TYPE_AUTO;

	//X.264 编码
	rc = x264_encoder_encode(en_h_, &nal, &i_nal, &en_picture_, &pic_out_);
	if (rc > 0){
		*out_size = rc;
		memcpy(out, nal[0].p_payload, rc);

		/*如果是intra_refresh方式，将每一帧都设置成关键帧来适应sim transport的缓冲buffer*/
		if (en_param_.b_intra_refresh == 1)
			*frame_type = 0x0002;
		else 
			*frame_type = pic_out_.i_type;

		rate_stat_update(&rate_stat_, rc, GET_SYS_MS());
		ret = true;
	}

	return ret;
}
