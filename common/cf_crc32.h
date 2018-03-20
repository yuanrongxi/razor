/*
 * crc32.h
 *
 *  Created on: Nov 10, 2016
 *      Author: lqp
 */

#ifndef COMMON_CRC32_H_
#define COMMON_CRC32_H_

#include <stdint.h>
#include <stddef.h>

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif /* COMMON_CRC32_H_ */
