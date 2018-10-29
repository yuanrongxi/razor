/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __echo_h264_decoder_h__
#define __echo_h264_decoder_h__

#include "codec_common.h"

/*H264½âÂëÆ÷*/
class H264Decoder : public VideoDecoder
{
public:
	H264Decoder();
	virtual ~H264Decoder();

protected:
	void set_codec_id();
};

#endif


