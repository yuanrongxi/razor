#include "echo_h265_encoder.h"

H265Encoder::H265Encoder()
{
	sws_context_ = NULL;
	pic_out_ = NULL;
	en_picture_ = NULL;
	en_h_ = NULL;
	en_param_ = NULL;

	src_width_ = PIC_WIDTH_640;
	src_height_ = PIC_HEIGHT_480;

	frame_rate_ = DEFAULT_FRAME_RATE;

	max_resolution_ = VIDEO_480P;
	curr_resolution_ = VIDEO_480P;
	bitrate_kbps_ = 640;

	buff_ = NULL;
	buff_size_ = 0;
}

H265Encoder::~H265Encoder()
{
	if (buff_ != NULL){
		free(buff_);
		buff_ = NULL;	
		buff_size_ = 0;
	}
}

bool H265Encoder::open_encoder()
{
	payload_type_ = codec_h265;
	en_param_ = x265_param_alloc();
	x265_param_default(en_param_);

	if (x265_param_default_preset(en_param_, "faster", "zerolatency") != 0)//medium,veryslow
		return false;

	//设置编码器参数
	config_param();

	//构造编码器
	if (x265_param_apply_profile(en_param_, "main") != 0)
		return false;

	if ((en_h_ = x265_encoder_open(en_param_)) == NULL)
		return false;

	const encoder_resolution_t& res = resolution_infos[curr_resolution_];

	if (buff_ == NULL){
		buff_ = (uint8_t*)malloc(res.codec_width * res.codec_height * 3 / 2);
		buff_size_ = res.codec_width * res.codec_height * 3 / 2;
	}
	else if (buff_size_ < res.codec_width * res.codec_height * 3 / 2){
		buff_ = (uint8_t*)realloc(buff_, buff_size_);
		buff_size_ = res.codec_width * res.codec_height * 3 / 2;
	}

	en_picture_ = x265_picture_alloc();
	x265_picture_init(en_param_, en_picture_);
	en_picture_->planes[0] = buff_;
	en_picture_->planes[1] = buff_ + res.codec_width *  res.codec_height;
	en_picture_->planes[2] = buff_ + res.codec_width * res.codec_height * 5 / 4;
	en_picture_->stride[0] = res.codec_width;
	en_picture_->stride[1] = res.codec_width / 2;
	en_picture_->stride[2] = res.codec_width / 2;

	pic_out_ = x265_picture_alloc();

	sws_context_ = sws_getContext(src_width_, src_height_,
		PIX_FMT_BGR24, res.codec_width, res.codec_height,
		PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	return true;
}

void H265Encoder::close_encoder()
{
	if (en_h_ != NULL){
		x265_picture_free(en_picture_);
		x265_picture_free(pic_out_);
		x265_encoder_close(en_h_);
		x265_param_free(en_param_);

		sws_freeContext(sws_context_);

		sws_context_ = NULL;
		en_h_ = NULL;

	}

	if (buff_ != NULL){
		free(buff_);
		buff_ = NULL;
		buff_size_ = 0;
	}
}

int H265Encoder::get_bitrate() const
{
	return en_param_->rc.vbvMaxBitrate;
}

int H265Encoder::get_codec_width() const
{
	return resolution_infos[curr_resolution_].codec_width;
}

int H265Encoder::get_codec_height() const
{
	return resolution_infos[curr_resolution_].codec_height;
}

void H265Encoder::reconfig_encoder(uint32_t bitrate_kbps)
{
	const encoder_resolution_t& res = resolution_infos[curr_resolution_];
	bitrate_kbps_ = bitrate_kbps;

	en_param_->rc.vbvMaxBitrate = bitrate_kbps;
	en_param_->rc.bitrate = (bitrate_kbps + res.min_rate) / 2;
	if (bitrate_kbps != en_param_->rc.vbvMaxBitrate){
		if (en_param_->rc.bitrate < res.min_rate)
			bitrate_kbps = res.min_rate;

		/*从新配置x.265的码率,及时生效*/
		if (en_h_ != NULL)
			x265_encoder_reconfig(en_h_, en_param_);
	}
}

void H265Encoder::config_param()
{
	const encoder_resolution_t& res = resolution_infos[curr_resolution_];

	en_param_->frameNumThreads = 4;
	en_param_->sourceWidth = res.codec_width;
	en_param_->sourceHeight = res.codec_height;

	en_param_->fpsNum = frame_rate_;
	en_param_->fpsDenom = 1;
	en_param_->rc.qpStep = 4;
	en_param_->rc.rateControlMode = X265_RC_CRF;

	en_param_->internalCsp = X265_CSP_I420;

	en_param_->logLevel = X265_LOG_NONE;

	en_param_->rc.qpMin = 5;
	en_param_->rc.qpMax = 36;
	en_param_->rc.rfConstant = 25;
	en_param_->rc.bitrate = res.min_rate;
	en_param_->rc.vbvMaxBitrate = res.max_rate;
	en_param_->keyframeMin = frame_rate_ * KEY_FRAME_SEC;
	en_param_->keyframeMax = frame_rate_ * KEY_FRAME_SEC;
	en_param_->bframes = 0;

	en_param_->rc.vbvBufferSize = 800;
	en_param_->rc.vbvBufferInit = 0.9;

	en_param_->bRepeatHeaders = 1;//write sps,pps before keyframe
	en_param_->bAnnexB = 1;

	en_param_->maxNumReferences = 4;
	en_param_->vui.bEnableVideoFullRangeFlag = 1;
	//m_en_param->i_sync_lookahead = 0;
	en_param_->vui.colorPrimaries = 2;
	en_param_->vui.transferCharacteristics = 2;
	en_param_->vui.matrixCoeffs = 2;
	//m_en_param->vui.i_chroma_loc = 2;
	//m_en_param->i_cabac_init_idc = 2;
	en_param_->rc.aqMode = X265_AQ_VARIANCE;
	en_param_->rc.aqStrength = 1.3;
	en_param_->rc.qCompress = 0.75;

	en_param_->bSaoNonDeblocked = 0x00000800;
	en_param_->deblockingFilterTCOffset = 2;
	en_param_->deblockingFilterBetaOffset = 2;
	en_param_->searchMethod = X265_DIA_SEARCH;
	//m_en_param->analyse.inter = X265_ANALYSE_I8x8 | X265_ANALYSE_I4x4;
	en_param_->bEnableRectInter = 1;
}

bool H265Encoder::encode(uint8_t *in, int in_size, enum PixelFormat pix_fmt, uint8_t *out, int *out_size, int *frame_type, bool request_keyframe)
{
	int rc;
	bool ret = false;

	//return ret;

	if (en_h_ == NULL || sws_context_ == NULL)
		return ret;

	try_change_resolution();

	en_param_->totalFrames++;
	frame_index_++;

	int stride = src_width_ * 3;
	rc = sws_scale(sws_context_, &in, &stride, 0, src_height_,
		(uint8_t**)en_picture_->planes, en_picture_->stride);


	en_picture_->pts = (int64_t)en_param_->totalFrames * en_param_->fpsDenom;

	x265_nal* nal = NULL;
	uint32_t i_nal = 0, i;

	//X.265 编码
	rc = x265_encoder_encode(en_h_, &nal, &i_nal, en_picture_, pic_out_);

	*out_size = 0;
	if (rc > 0){
		for (i = 0; i < i_nal; i++){
			memcpy(out + *out_size, nal[i].payload, nal[i].sizeBytes);
			*out_size += nal[i].sizeBytes;
		}

		*frame_type = IS_X265_TYPE_I(pic_out_->sliceType) ? 1 : 0;

		rate_stat_update(&rate_stat_, *out_size, GET_SYS_MS());
		ret = true;
	}

	return ret;
}


