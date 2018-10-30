#include "echo_h265_decoder.h"

H265Decoder::H265Decoder()
{
	codec_id_ = AV_CODEC_ID_H265;
}

H265Decoder::~H265Decoder()
{
	
}

void H265Decoder::set_codec_id()
{
	codec_id_ = AV_CODEC_ID_H265;
}