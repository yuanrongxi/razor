/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef _CF_PLATFORM_H
#define _CF_PLATFORM_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef WIN32
#include <windows.h>

#pragma warning(disable: 4996) //this is about sprintf, snprintf, vsnprintf

#define snprintf _snprintf

#ifndef inline
#define	inline __inline
#endif

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* su_thread;
typedef void* (*su_thread_fun)(void* data);
typedef void* su_mutex; 
typedef void* su_cond;
typedef void* su_queue;
typedef void* su_rwlock;
typedef int	su_socket;
typedef struct sockaddr_in  su_addr;

typedef void(*watch_func_type)();

int			su_platform_init();
int			su_platform_uninit();

su_thread	su_create_thread(char* name, su_thread_fun func, void* data);
void		su_destroy_thread(su_thread thread);

su_mutex	su_create_mutex();
void		su_mutex_lock(su_mutex mutex);
int			su_mutex_trylock(su_mutex mutex);
void		su_mutex_unlock(su_mutex mutex);
void		su_destroy_mutex(su_mutex mutex);

su_cond		su_create_cond();
void		su_cond_wait(su_cond cond);
void		su_cond_signal(su_cond cond);
void		su_destroy_cond(su_cond cond);

void 		su_sleep(uint64_t seconds, uint64_t micro_seconds);

int64_t		su_get_sys_time();

int			su_udp_create(const char* ip, uint16_t port, su_socket* fd);
int			su_tcp_create(su_socket* fd);
int			su_tcp_listen_create(uint16_t port, su_socket* fd);

void		su_set_addr(su_addr* addr, const char* ip, uint16_t port);

void		su_set_addr2(su_addr* addr, uint32_t ip, uint16_t port);

int32_t		su_udp_send(su_socket fd, su_addr* addr, void* buf, uint32_t len);
int32_t		su_udp_recv(su_socket fd, su_addr* addr, void* buf, uint32_t len, uint32_t ms);

int			su_socket_noblocking(int fd);
void		su_socket_destroy(su_socket fd);

uint32_t	su_get_local_ip();

char*		su_addr_to_string(su_addr* addr, char* str, int len);
void		su_string_to_addr(su_addr* addr, char* str);
void 		su_addrstr_to_iport(char* in, char* ip, int* pport);
char* 		su_addr_to_iport(su_addr* addr, char* str, int len, uint16_t* port);
void		su_addr_to_addr(su_addr* src, su_addr* dst);
int			su_addr_cmp(su_addr* src, su_addr* dst);
int			su_addr_eq(su_addr* src, su_addr* dst);

#define IP_SIZE 32

#define SU_MAX(a, b)		((a) > (b) ? (a) : (b))
#define SU_MIN(a, b)		((a) < (b) ? (a) : (b))	
#define SU_ABS(a, b)		((a) > (b) ? ((a) - (b)) : ((b) - (a)))

#define GET_SYS_MS()		(su_get_sys_time() / 1000)

#ifdef __cplusplus
}
#endif

#endif
