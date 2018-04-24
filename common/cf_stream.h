/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __CF_STREAM_H_
#define __CF_STREAM_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BIN_SIZE		(10485760 * 10)
#define READ_DATA_ERROR		0xffff
#define MAX_MTU_SIZE		1476

typedef struct bin_stream_s
{
	uint8_t*	data;
	size_t		size;

	uint8_t*	rptr;
	size_t		rsize;

	uint8_t*	wptr;
	size_t		used;
	
	uint64_t	magic;
}bin_stream_t;

int32_t		bin_stream_init(void* ptr); 
void		bin_stream_destroy(void* ptr);
void		bin_stream_reset(void* ptr, int32_t flag);
int32_t		bin_stream_check(void* ptr);

void		bin_stream_rewind(bin_stream_t* strm, int32_t reset);
void		bin_stream_resize(bin_stream_t* strm, size_t new_size);
int 		bin_stream_available_read(bin_stream_t* strm);
void		bin_stream_reduce(bin_stream_t* strm);
void		bin_stream_set_used_size(bin_stream_t* strm, size_t used);
void		bin_stream_move(bin_stream_t* strm);

void		mach_uint8_write(bin_stream_t* strm, uint8_t val);
void		mach_uint8_read(bin_stream_t* strm, uint8_t* val);

void		mach_int8_write(bin_stream_t* strm, int8_t val);
void		mach_int8_read(bin_stream_t* strm, int8_t* val);	

void		mach_uint16_write(bin_stream_t* strm, uint16_t val);
void		mach_uint16_read(bin_stream_t* strm, uint16_t* val);

void		mach_int16_write(bin_stream_t* strm, int16_t val);
void		mach_int16_read(bin_stream_t* strm, int16_t* val);	

void		mach_uint32_write(bin_stream_t* strm, uint32_t val);
void		mach_uint32_read(bin_stream_t* strm, uint32_t* val);

void		mach_int32_write(bin_stream_t* strm, int32_t val);
void		mach_int32_read(bin_stream_t* strm, int32_t* val);	

void		mach_uint64_write(bin_stream_t* strm, uint64_t val);
void		mach_uint64_read(bin_stream_t* strm, uint64_t* val);

void		mach_int64_write(bin_stream_t* strm, int64_t val);
void		mach_int64_read(bin_stream_t* strm, int64_t* val);

void		mach_data_write(bin_stream_t* strm, const uint8_t* str, size_t size);
uint16_t	mach_data_read(bin_stream_t* strm, uint8_t* str, size_t size);

void		mach_block_write(bin_stream_t* strm, const uint8_t* str, size_t size);
uint16_t	mach_block_read(bin_stream_t* strm, uint8_t* str, size_t size);

void		mach_put_2(uint8_t* ptr, uint16_t val);
uint16_t	mach_get_2(uint8_t* ptr);

void		mach_put_4(uint8_t* ptr, uint32_t val);
uint32_t	mach_get_4(uint8_t* ptr);

void		mach_put_8(uint8_t* ptr, uint64_t val);
uint64_t	mach_get_8(uint8_t* ptr);

#endif


