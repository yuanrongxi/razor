/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __h264_encoder_h__
#define __h264_encoder_h__

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

/*h264编码器，支持自动缩放编码,支持动态修改码率，不支持动态修改分辨率。为了更好的和razor进行配合传输，编码器被设置成inter-refresh模式*/
class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();

	bool init(int frame_rate, int src_width, int src_height, int dst_width, int dst_height);
	void destroy();

	bool encode(uint8_t *in, int in_size, enum PixelFormat pix_fmt, uint8_t *out, int *out_size, int *frame_type);

	void set_bitrate(uint32_t bitrate_kbps);
	uint32_t get_bitrate() const;

private:
	void config_param();

private:
	unsigned int	codec_width_;			// Output Width
	unsigned int	codec_height_;			// Output Height

	unsigned int	src_width_;				// Input Width
	unsigned int	src_height_;			// Input Height
	
	int				frame_rate_;			// frame

	int				resolution_;			//编码分辨率类型

	bool			inited_;

	SwsContext*		sws_context_;

	//X264对象参数
	x264_picture_t	pic_out_;
	x264_picture_t	en_picture_;
	x264_t *		en_h_;
	x264_param_t	en_param_;

	unsigned int	max_rate_;
	unsigned int	min_rate_;
};

#endif


