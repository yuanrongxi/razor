/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __echo_h264_decoder_h__
#define __echo_h264_decoder_h__

#ifdef _MSC_VER	/* MSVC */
#pragma warning(disable: 4996)
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strcasecmp	stricmp
#define strncasecmp strnicmp
#endif

#include <stdint.h>

extern "C"
{
#include "x264.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
};

#include "echo_h264_common.h"

/*H264½âÂëÆ÷*/
class H264Decoder
{
public:
	H264Decoder();
	~H264Decoder();

	bool init();
	void destroy();

	bool decode(uint8_t *in, int in_size, uint8_t **out, int &out_width, int& out_height, int32_t &pict_type);
	uint32_t get_width() const { return curr_width_; };
	uint32_t get_height() const { return curr_height_; };

private:
	bool				inited_;

	int					curr_width_;
	int					curr_height_;

	uint8_t*            buff_;
	uint32_t			buff_size_;

	AVFrame*			de_frame_;
	AVCodec*			de_codec_;
	AVCodecContext*		de_context_;

	AVPacket			avpkt_;
	AVPicture			outpic_;

	SwsContext*			sws_context_;
};

#endif


