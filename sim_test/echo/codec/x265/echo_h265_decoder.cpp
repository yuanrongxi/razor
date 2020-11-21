/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

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