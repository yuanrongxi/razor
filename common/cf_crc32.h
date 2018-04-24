/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#ifndef COMMON_CRC32_H_
#define COMMON_CRC32_H_

#include <stdint.h>
#include <stddef.h>

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif /* COMMON_CRC32_H_ */
