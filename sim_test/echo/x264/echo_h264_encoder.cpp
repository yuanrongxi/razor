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

typedef struct
{
	int			resolution;				/*分辨率*/
	int			codec_width;			/*编码图像宽度*/
	int			codec_height;			/*编码图像高度*/
	uint32_t	max_rate;				/*最大码率，kbps*/
	uint32_t	min_rate;				/*最小码率, kbps*/
}encoder_resolution_t;

static encoder_resolution_t resolution_infos[RESOLUTIONS_NUMBER] = {
	{ VIDEO_120P, PIC_WIDTH_160, PIC_HEIGHT_120, 64, 32 },
	{ VIDEO_240P, PIC_WIDTH_320, PIC_HEIGHT_240, 200, 64 },
	{ VIDEO_360P, PIC_WIDTH_480, PIC_HEIGHT_360, 480, 120 },
	{ VIDEO_480P, PIC_WIDTH_640, PIC_HEIGHT_480, 1000, 320},
	{ VIDEO_640P, PIC_WIDTH_960, PIC_HEIGHT_640, 1600, 640 },
	{ VIDEO_720P, PIC_WIDTH_1280, PIC_HEIGHT_720, 2400, 1200 },
	{ VIDEO_1080P, PIC_WIDTH_1920, PIC_HEIGHT_1080, 4000, 1600 },
};

H264Encoder::H264Encoder()
{
	src_width_ = PIC_WIDTH_640;
	src_height_ = PIC_HEIGHT_480;

	sws_context_ = NULL;

	frame_rate_ = DEFAULT_FRAME_RATE;
	max_resolution_ = VIDEO_240P;
	curr_resolution_ = VIDEO_240P;

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

	frame_rate_ = frame_rate;

	/*确定支持的分辨率*/
	if (dst_width == PIC_WIDTH_1920 && dst_height == PIC_HEIGHT_1080)
		max_resolution_ = VIDEO_1080P;
	else if (dst_width == PIC_WIDTH_1280 && dst_height == PIC_HEIGHT_720)
		max_resolution_ = VIDEO_720P;
	else if (dst_width == PIC_WIDTH_960 && dst_height == PIC_HEIGHT_640)
		max_resolution_ = VIDEO_640P;
	else if (dst_width == PIC_WIDTH_640 && dst_height == PIC_HEIGHT_480)
		max_resolution_ = VIDEO_480P;
	else if (dst_width == PIC_WIDTH_480 && dst_height == PIC_HEIGHT_360)
		max_resolution_ = VIDEO_360P;
	else if (dst_width == PIC_WIDTH_320 && dst_height == PIC_HEIGHT_240)
		max_resolution_ = VIDEO_240P;
	else if (dst_width == PIC_WIDTH_160 && dst_height == PIC_HEIGHT_120)
		max_resolution_ = VIDEO_120P;
	else
		return false;

	curr_resolution_ = max_resolution_;
	bitrate_kbps_ = (resolution_infos[curr_resolution_].max_rate + resolution_infos[curr_resolution_].min_rate) / 2;

	if (!open_encoder())
		return false;

	inited_ = true;

	return true;
}

void H264Encoder::destroy()
{
	if (!inited_)
		return;

	inited_ = false;
	close_encoder();

	max_resolution_ = VIDEO_240P;
	curr_resolution_ = VIDEO_240P;
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
}

void H264Encoder::set_bitrate(uint32_t bitrate_kbps)
{
	bitrate_kbps_ = bitrate_kbps;

	if (bitrate_kbps > resolution_infos[curr_resolution_].max_rate)
		bitrate_kbps = resolution_infos[curr_resolution_].max_rate;
	else if (bitrate_kbps < resolution_infos[curr_resolution_].min_rate)
		bitrate_kbps = resolution_infos[curr_resolution_].min_rate;

	if (bitrate_kbps != en_param_.rc.i_vbv_max_bitrate){
		en_param_.rc.i_vbv_max_bitrate = bitrate_kbps;
		en_param_.rc.i_bitrate = bitrate_kbps - bitrate_kbps / 4;
		if (en_param_.rc.i_bitrate < resolution_infos[curr_resolution_].min_rate)
			bitrate_kbps = resolution_infos[curr_resolution_].min_rate;

		/*从新配置x.264的码率,及时生效*/
		if (en_h_ != NULL)
			x264_encoder_reconfig(en_h_, &en_param_);
	}
}

uint32_t H264Encoder::get_bitrate() const
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

#define KEY_FRAME_SEC 4
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

	en_param_.rc.i_qp_min = 5;
	en_param_.rc.i_qp_max = 40;
	en_param_.rc.i_qp_constant = 24;
	en_param_.rc.i_bitrate = res.min_rate;
	en_param_.rc.i_vbv_max_bitrate = (res.min_rate + res.max_rate) / 2;
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

void H264Encoder::try_change_resolution()
{
	/*判断下一帧处在gop的位置, 如果处于后半段，我们可以尝试改变分辨率*/
	if (en_param_.i_frame_total > frame_rate_ * KEY_FRAME_SEC * 2){
		uint32_t frame_index = (en_param_.i_frame_total + 1) % (frame_rate_ * KEY_FRAME_SEC);
		if (frame_index >= ((KEY_FRAME_SEC / 2) * frame_rate_)){
			const encoder_resolution_t& res = resolution_infos[curr_resolution_];
			if (res.min_rate > bitrate_kbps_ && curr_resolution_ > VIDEO_120P){
				/*降低一层分辨率*/
				curr_resolution_--;
				close_encoder();
				open_encoder();
			}
			else if (res.max_rate < bitrate_kbps_ && curr_resolution_ + 1 <= max_resolution_){
				/*升高一层分辨率*/
				curr_resolution_++;
				close_encoder();
				open_encoder();
			}
		}
	}
}

bool H264Encoder::encode(uint8_t *in, int in_size, enum PixelFormat src_pix_fmt, uint8_t *out, int *out_size, int *frame_type, bool request_keyframe)
{
	if (!inited_)
		return false;

	int ret;
	static AVPicture pic = { 0 };

	try_change_resolution();

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

	/*指定编码输出关键帧*/
	if (request_keyframe)
		en_picture_.i_type = X264_TYPE_KEYFRAME;

	//X.264 编码
	ret = x264_encoder_encode(en_h_, &nal, &i_nal, &en_picture_, &pic_out_);
	if (ret > 0){
		*out_size = ret;
		memcpy(out, nal[0].p_payload, ret);

		/*如果是intra_refresh方式，将每一帧都设置成关键帧来适应sim transport的缓冲buffer*/
		if (en_param_.b_intra_refresh == 1)
			*frame_type = 0x0002;
		else 
			*frame_type = pic_out_.i_type;
		return true;
	}

	return false;
}
