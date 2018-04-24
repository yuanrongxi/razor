/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __CF_HEX_H_
#define __CF_HEX_H_

#include <stdint.h>

char* wb_bin2asc(const uint8_t *in, int32_t in_size, char* out, int32_t out_size);

void wb_asc2bin(const char* in, int32_t in_size, uint8_t* out, int32_t out_size, int32_t* out_length);

#endif


