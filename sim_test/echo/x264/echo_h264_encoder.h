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

/*h264编码器，支持自动缩放编码,支持动态修改码率，动态修改分辨率。为了更好的和razor进行配合传输，编码器被设置成inter-refresh模式*/
class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();

	bool init(int frame_rate, int src_width, int src_height, int dst_width, int dst_height);
	void destroy();

	bool encode(uint8_t *in, int in_size, enum PixelFormat pix_fmt, uint8_t *out, int *out_size, int *frame_type, bool request_keyframe = false);

	void set_bitrate(uint32_t bitrate_kbps);
	uint32_t get_bitrate() const;

	int get_codec_width() const;
	int get_codec_height() const;

private:
	void config_param();
	bool open_encoder();
	void close_encoder();

	void try_change_resolution();

	int find_resolution(uint32_t birate_kpbs);

private:
	unsigned int	src_width_;				// Input Width
	unsigned int	src_height_;			// Input Height
	
	int				frame_rate_;			// frame
	uint32_t		bitrate_kbps_;			// 网络反馈的码率

	int				max_resolution_;		// 初始编码分辨率,用户指定的最大分辨率
	int				curr_resolution_;		// 当前编码器所处的分辨率

	bool			inited_;

	//RGB -> YUV转换对象
	SwsContext*		sws_context_;

	//X264对象参数
	x264_picture_t	pic_out_;
	x264_picture_t	en_picture_;
	x264_t *		en_h_;
	x264_param_t	en_param_;
};

#endif


