/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <string.h>
#include <stdlib.h>
#include "cf_stream.h"
#include <assert.h>

#define DEFAULT_PACKET_SIZE 1024
#define BIN_STREAM_MAGIC	0x019ef8f09867cda6

#define RESIZE(s)	\
	if(strm->size < strm->used + (s)) \
		bin_stream_resize(strm, strm->used + (s))


static int32_t	big_endian = -1;

int32_t bin_stream_init(void* ptr)
{
	bin_stream_t* strm;

	if(big_endian == -1){
		union 
		{
			uint16_t	s16;
			uint8_t		s8[2];
		}un;

		un.s16 = 0x0100; 
		if(un.s8[0] == 0x01)
			big_endian = 1;
		else 
			big_endian = 0;
	}

	strm = (bin_stream_t *)ptr;
	strm->size = sizeof(uint8_t) * DEFAULT_PACKET_SIZE;
	strm->data = (uint8_t *)malloc(strm->size);
	if(strm->data == NULL)
		return -1;

	strm->rptr = strm->data;
	strm->wptr = strm->data;

	strm->rsize = 0;
	strm->used = 0;

	strm->magic = 0;

	return 0;
} 

void bin_stream_destroy(void* ptr)
{
	if(ptr != NULL){
		bin_stream_t* strm = (bin_stream_t *)ptr;
		free(strm->data);
		strm->size = 0;

		strm->rptr = NULL;
		strm->rsize = 0;

		strm->wptr = NULL;
		strm->used = 0;
	}
}

void bin_stream_reset(void* ptr, int32_t flag)
{
	bin_stream_t* strm = (bin_stream_t*)ptr;
	if(flag == 1)
		strm->magic = BIN_STREAM_MAGIC;
	else
		strm->magic = 0;

	bin_stream_rewind(strm, 1);
	bin_stream_reduce(strm);
}

int32_t bin_stream_check(void* ptr)
{
	bin_stream_t* strm = (bin_stream_t*)ptr;
	if(strm->magic == BIN_STREAM_MAGIC)
		return 0;
	
	return -1;
}

void bin_stream_rewind(bin_stream_t* strm, int32_t reset)
{
	if(reset == 1){
		strm->wptr = strm->data;
		strm->used = 0;
	}

	strm->rptr = strm->data;
	strm->rsize = 0;
}

int bin_stream_available_read(bin_stream_t* strm)
{
	assert(strm->rsize <= strm->used);

	return strm->used - strm->rsize;
}

void bin_stream_resize(bin_stream_t* strm, size_t new_size)
{
	size_t alloc_size;

	if(new_size <= strm->size)
		return ;

	alloc_size = strm->size;

	while(new_size > alloc_size)
		alloc_size = alloc_size * 2;

	strm->data = (uint8_t*)realloc(strm->data, alloc_size);
	strm->size = alloc_size;

	strm->wptr = (uint8_t *)strm->data + strm->used;
	strm->rptr = (uint8_t *)strm->data + strm->rsize;
}

void bin_stream_set_used_size(bin_stream_t* strm, size_t used)
{
	strm->used = used;
}

void bin_stream_reduce(bin_stream_t* strm)
{
	if (strm->size > DEFAULT_PACKET_SIZE * 2){
		strm->data = (uint8_t*)realloc(strm->data, DEFAULT_PACKET_SIZE);
		strm->rptr = strm->data;
		strm->wptr = strm->data;

		strm->size = DEFAULT_PACKET_SIZE;
		strm->rsize = 0;
		strm->used = 0;
	}
}

void bin_stream_move(bin_stream_t* strm)
{
	if(strm->used == strm->rsize && strm->rsize > 0){ 
		strm->rptr = strm->data;
		strm->wptr = strm->data;

		strm->used = 0;
		strm->rsize = 0;
	}

	if(strm->used > strm->rsize && strm->rsize > (strm->size / 2) && strm->used + 256 >= strm->size){ 
		uint32_t size = strm->used - strm->rsize;
		memmove(strm->data, strm->rptr, size);

		strm->wptr = strm->data + size;
		strm->rptr = strm->data;
		strm->rsize = 0;
		strm->used = size;
	}
}

void mach_uint8_write(bin_stream_t* strm, uint8_t val)
{
	RESIZE(1);

	*(strm->wptr ++) = val;
	strm->used ++;
}

void mach_uint8_read(bin_stream_t* strm, uint8_t* val)
{	
	if(strm->used < strm->rsize + 1){
		*val = 0;
	}
	else{
		*val = *(strm->rptr ++);
		strm->rsize ++;
	}
}

void mach_int8_write(bin_stream_t* strm, int8_t val)
{
	RESIZE(1);

	*(strm->wptr ++) = (uint8_t)val;
	strm->used ++;
}

void mach_int8_read(bin_stream_t* strm, int8_t* val)
{
	if(strm->used < strm->rsize + 1){
		*val = 0;
	}
	else{
		*val = (int8_t)(*(strm->rptr ++));
		strm->rsize ++;
	}
}

void mach_uint16_write(bin_stream_t* strm, uint16_t val)
{
	RESIZE(2);
	mach_put_2(strm->wptr, val);
	strm->wptr += 2;
	strm->used += 2;
}

void mach_uint16_read(bin_stream_t* strm, uint16_t* val)
{
	if(strm->used < strm->rsize + 2){
		*val = 0;
	}
	else{
		*val = (int16_t)mach_get_2(strm->rptr);
		strm->rptr += 2;
		strm->rsize += 2;
	}
}

void mach_int16_write(bin_stream_t* strm, int16_t val)
{
	RESIZE(2);
	mach_put_2(strm->wptr, (uint16_t)val);
	strm->wptr += 2;
	strm->used += 2;
}

void mach_int16_read(bin_stream_t* strm, int16_t* val)
{
	if(strm->used < strm->rsize + 2){
		*val = 0;
	}
	else{
		*val = (int16_t)mach_get_2(strm->rptr);
		strm->rptr += 2;
		strm->rsize += 2;
	}
}

void mach_uint32_write(bin_stream_t* strm, uint32_t val)
{
	RESIZE(4);
	mach_put_4(strm->wptr, val);
	strm->wptr += 4;
	strm->used += 4;
}

void mach_uint32_read(bin_stream_t* strm, uint32_t* val)
{
	if(strm->used < strm->rsize + 4){
		*val = 0;
	}
	else{
		*val = mach_get_4(strm->rptr);
		strm->rptr += 4;
		strm->rsize += 4;
	}
}

void mach_int32_write(bin_stream_t* strm, int32_t val)
{
	RESIZE(4);
	mach_put_4(strm->wptr, (uint32_t)val);
	strm->wptr += 4;
	strm->used += 4;
}

void mach_int32_read(bin_stream_t* strm, int32_t* val)
{
	if(strm->used < strm->rsize + 4){
		*val = 0;
	}
	else{
		*val = (int32_t)mach_get_4(strm->rptr);
		strm->rptr += 4;
		strm->rsize += 4;
	}
}

void mach_uint64_write(bin_stream_t* strm, uint64_t val)
{
	RESIZE(8);
	mach_put_8(strm->wptr, val);
	strm->wptr += 8;
	strm->used += 8;
}

void mach_uint64_read(bin_stream_t* strm, uint64_t* val)
{
	if(strm->used < strm->rsize + 8){
		*val = 0;
	}
	else{
		*val = mach_get_8(strm->rptr);
		strm->rptr += 8;
		strm->rsize += 8;
	}
}

void mach_int64_write(bin_stream_t* strm, int64_t val)
{
	RESIZE(8);
	mach_put_8(strm->wptr, (uint64_t)val);
	strm->wptr += 8;
	strm->used += 8;
}

void mach_int64_read(bin_stream_t* strm, int64_t* val)
{
	if(strm->used < strm->rsize + 8){
		*val = 0;
	}
	else{
		*val = mach_get_8(strm->rptr);
		strm->rptr += 8;
		strm->rsize += 8;
	}
}

void mach_data_write(bin_stream_t* strm, const uint8_t* str, size_t size)
{
	RESIZE(size + 2);
	mach_uint16_write(strm, (uint16_t)size);

	memcpy(strm->wptr, str, size);

	strm->wptr += size;
	strm->used += size;
}

uint16_t mach_data_read(bin_stream_t* strm, uint8_t* str, size_t size)
{
	uint16_t len;
	mach_uint16_read(strm, &len);

	if(len > size || strm->rsize + len > strm->used)
		return READ_DATA_ERROR;

	memcpy(str, strm->rptr, len);

	strm->rptr += len;
	strm->rsize += len;

	return len;
}

void mach_block_write(bin_stream_t* strm, const uint8_t* str, size_t size) {
	RESIZE(size);

	memcpy(strm->wptr, str, size);

	strm->wptr += size;
	strm->used += size;
}

uint16_t mach_block_read(bin_stream_t* strm, uint8_t* str, size_t size) {
	if(strm->rsize + size > strm->used)
		return READ_DATA_ERROR;

	memcpy(str, strm->rptr, size);

	strm->rptr += size;
	strm->rsize += size;

	return size;
}

void mach_put_2(uint8_t* ptr, uint16_t val)
{
	if(big_endian){
		*((uint16_t*)ptr) = val;
	}
	else{
		ptr[0] = (uint8_t)(val >> 8);
		ptr[1] = (uint8_t)val;
	}
}

uint16_t mach_get_2(uint8_t* ptr)
{
	if(big_endian)
		return *((uint16_t*)(ptr));
	else
		return ((uint16_t)(ptr[0]) << 8) + (uint32_t)(ptr[1]);
}

void mach_put_4(uint8_t* ptr, uint32_t val)
{
	if(big_endian){
		*((uint32_t*)ptr) = val;
	}
	else{
		ptr[0] = (uint8_t)(val >> 24);
		ptr[1] = (uint8_t)(val >> 16);
		ptr[2] = (uint8_t)(val >> 8);
		ptr[3] = (uint8_t)val;
	}
}

uint32_t mach_get_4(uint8_t* ptr)
{
	if(big_endian)
		return *((uint32_t*)(ptr));
	else
		return ((uint32_t)(ptr[0]) << 24) + ((uint32_t)(ptr[1]) << 16) + ((uint32_t)(ptr[2]) << 8) + (uint32_t)(ptr[3]);
}

void mach_put_8(uint8_t* ptr, uint64_t val)
{
	if(big_endian)
		*((uint64_t*)ptr) = val;
	else{
		ptr[0] = (uint8_t)(val >> 56);
		ptr[1] = (uint8_t)(val >> 48);
		ptr[2] = (uint8_t)(val >> 40);
		ptr[3] = (uint8_t)(val >> 32);
		ptr[4] = (uint8_t)(val >> 24);
		ptr[5] = (uint8_t)(val >> 16);
		ptr[6] = (uint8_t)(val >> 8);
		ptr[7] = (uint8_t)val;
	}
}

uint64_t mach_get_8(uint8_t* ptr)
{
	if(big_endian)
		return *((uint64_t*)(ptr));
	else
		return ((uint64_t)(ptr[0]) << 56) + ((uint64_t)(ptr[1]) << 48) + ((uint64_t)(ptr[2]) << 40) + ((uint64_t)(ptr[3]) << 32)
		+ ((uint64_t)(ptr[4]) << 24) + ((uint64_t)(ptr[5]) << 16) + ((uint64_t)(ptr[6]) << 8) + (uint64_t)(ptr[7]);
}

