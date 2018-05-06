/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __GETTIMEOFDAY_H
#define __GETTIMEOFDAY_H
#ifdef WIN32
//ÉêÃ÷ÒýÓÃ
int gettimeofday(struct timeval *tv, void *tz);
#endif
#endif