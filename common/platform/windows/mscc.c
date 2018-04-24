/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h> 
#include "cf_platform.h"

/*fuck vs-2013*/
#pragma warning(disable: 4996)

int su_platform_init()
{
	struct WSAData wsa_data_;
	WSAStartup(MAKEWORD(2, 2), &wsa_data_);
	return 0;
}

int su_platform_uninit()
{
	WSACleanup();
	return 0;
}

su_thread su_create_thread(char* name, su_thread_fun func, void* data)
{
	return (su_thread)_beginthreadex(NULL, 0, (unsigned int(_stdcall*)(void*))func, data,
		0, NULL);
}

void su_destroy_thread(su_thread thread)
{
	return;
}

su_mutex su_create_mutex()
{
	CRITICAL_SECTION* m = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(m);
	return (su_mutex)m;
}

void su_mutex_lock(su_mutex mutex)
{
	EnterCriticalSection((CRITICAL_SECTION*)mutex);
}

int su_mutex_trylock(su_mutex mutex)
{
	return !TryEnterCriticalSection((CRITICAL_SECTION*)mutex);
}

void su_mutex_unlock(su_mutex mutex)
{
	LeaveCriticalSection((CRITICAL_SECTION*)mutex);
}
void su_destroy_mutex(su_mutex mutex)
{
	DeleteCriticalSection((CRITICAL_SECTION*)mutex);
	free(mutex);
}

typedef struct
{
	HANDLE  event;
}su_cond_t;

su_cond su_create_cond()
{
	su_cond_t* cond = (su_cond_t *)malloc(sizeof(su_cond_t));
	cond->event = CreateEvent(NULL, FALSE, FALSE, NULL);

	return (su_cond *)cond;
}

void su_cond_wait(su_cond cond)
{
	su_cond_t* c = (su_cond_t *)cond;
	WaitForSingleObject(c->event, INFINITE);
}

void su_cond_signal(su_cond cond)
{
	su_cond_t* c = (su_cond_t *)cond;
	SetEvent(c->event);
}

void su_destroy_cond(su_cond cond)
{
	su_cond_t* c = (su_cond_t *)cond;
	free(c);
}

int64_t su_get_sys_time()
{
	FILETIME ft;
	int64_t t;
	GetSystemTimeAsFileTime(&ft);
	t = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
	return t / 10 - 11644473600000000; /* Jan 1, 1601 */
}

int su_udp_create(const char* ip, uint16_t port, su_socket* fd)
{
	SOCKET s;
	SOCKADDR_IN addr;
	int buf_size = 1024 * 64;
	s = socket(AF_INET,SOCK_DGRAM,0);

	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int));

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family 	= AF_INET;
	addr.sin_port       = htons(port);
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
	if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
		return -1;
	}

	*fd = s;
	return 0;
}

int su_tcp_create(su_socket* fd)
{
	int buf_size = 16 * 2014;
	int enable = 1;
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return -1;

	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int));

	*fd = s;

	return 0;
}

int su_tcp_listen_create(uint16_t port, su_socket* fd)
{
	int s;
	int buf_size;
	struct sockaddr_in addr;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return -1;

	buf_size = 16 * 1024;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int32_t));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int32_t));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0){
		printf("bind port failed! port = %u\n", port);
		closesocket(s);
		return -1;
	}

	if (listen(s, 1024) == -1){
		printf("socket listen failed!\n");
		closesocket(s);
		return -1;
	}

	*fd = s;

	return 0;
}

int su_socket_noblocking(int fd)
{
	int mode = 1;
	ioctlsocket(fd, FIONBIO, (u_long FAR*)&mode);

	return 0;
}

int32_t su_udp_send(su_socket fd, su_addr* addr, void* buf, uint32_t len)
{
	return sendto(fd, (const char*)buf, len, 0, (struct sockaddr*)addr, sizeof(struct sockaddr));
}

int32_t su_udp_recv(su_socket fd, su_addr* addr, void* buf, uint32_t len, uint32_t ms)
{
	int32_t ret = -1;
	if (ms > 0){
		fd_set fds;
		struct timeval timeout = { 0, ms * 1000 };

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		switch (select(fd + 1, &fds, NULL, NULL, &timeout))
		{ 
		case -1:
			return -1;
		case 0:
			return 0;
		default:
			break;
		}

		if (!(FD_ISSET(fd, &fds)))
			return -1;
	}

	int addr_len = sizeof(struct sockaddr_in);
	ret = recvfrom(fd, (char*)buf, len, 0, (struct sockaddr*)addr, (addr == NULL ? NULL : &addr_len));
	if (ret <= 0)
		return -1;

	return ret;
}

void su_socket_destroy(su_socket fd)
{
	closesocket(fd);
}

void su_set_addr(su_addr* addr, const char* ip, uint16_t port)
{
	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family		= AF_INET;
	addr->sin_port			= htons(port);
	addr->sin_addr.s_addr	= inet_addr(ip);
}

void su_set_addr2(su_addr* addr, uint32_t ip, uint16_t port)
{
	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(ip);
}

void su_addr_to_addr(su_addr* srcAddr, su_addr* dstAddr)
{
	dstAddr->sin_family = AF_INET;
	dstAddr->sin_port = srcAddr->sin_port;
	dstAddr->sin_addr.s_addr = srcAddr->sin_addr.s_addr;
}

char* su_addr_to_string(su_addr* addr, char* str, int len)
{
	uint8_t b1, b2, b3, b4;
	if (len < 24 || addr == NULL || str == NULL)
		return NULL;

	b1 = addr->sin_addr.S_un.S_un_b.s_b1;
	b2 = addr->sin_addr.S_un.S_un_b.s_b2;
	b3 = addr->sin_addr.S_un.S_un_b.s_b3;
	b4 = addr->sin_addr.S_un.S_un_b.s_b4;
	sprintf(str, "%u.%u.%u.%u:%u", b1, b2, b3, b4, ntohs(addr->sin_port));
	return str;
}

void su_string_to_addr(su_addr* addr, char* str)
{
	char* pos, ip[IP_SIZE] = { 0 };
	uint16_t port, i;

	if (addr == NULL || str == NULL)
		return;

	i = 0;
	pos = str;
	do{
		ip[i++] = *pos++;
	} while (*pos != ':' && *pos != '\0' && i < IP_SIZE - 1);

	if (*pos == ':')
		pos++;

	port = atoi(pos);
	su_set_addr(addr, ip, port);
}

char* su_addr_to_iport(su_addr* addr, char* str, int len, uint16_t* port)
{
	uint8_t b1, b2, b3, b4;
	if (len < 24 || addr == NULL || str == NULL)
		return NULL;

	b1 = addr->sin_addr.S_un.S_un_b.s_b1;
	b2 = addr->sin_addr.S_un.S_un_b.s_b2;
	b3 = addr->sin_addr.S_un.S_un_b.s_b3;
	b4 = addr->sin_addr.S_un.S_un_b.s_b4;
	sprintf(str, "%u.%u.%u.%u", b1, b2, b3, b4);
	*port = ntohs(addr->sin_port);
	return str;
}

int su_addr_cmp(su_addr* srcAddr, su_addr* dstAddr)
{
	if (srcAddr->sin_addr.s_addr > dstAddr->sin_addr.s_addr)
		return 1;
	else if (srcAddr->sin_addr.s_addr < dstAddr->sin_addr.s_addr)
		return -1;
	else if (srcAddr->sin_port > dstAddr->sin_port)
		return 1;
	else if (srcAddr->sin_port < dstAddr->sin_port)
		return -1;
	else
		return 0;
}

int su_addr_eq(su_addr* srcAddr, su_addr* dstAddr)
{
	if (srcAddr->sin_addr.s_addr == dstAddr->sin_addr.s_addr && srcAddr->sin_port == dstAddr->sin_port)
		return 0;

	return -1;
}

void su_sleep(uint64_t seconds, uint64_t micro_seconds)
{
	Sleep(seconds * 1000 + micro_seconds / 1000);
}

#define ARRAY_SIZE(x) (((sizeof(x)/sizeof(x[0]))))

#define IS_PRIVATE_IP(ip)							\
	((((ip) >> 24) == 127) || (((ip) >> 24) == 10)	\
	|| (((ip) >> 20) == ((172 << 4) | 1))			\
	|| (((ip) >> 16) == ((192 << 8) | 168)))

#define IS_VPN(ip)	((((ip) >> 16) & 0x00ff) > 250)

uint32_t su_get_local_ip()
{
	char hostname[256];
	struct hostent* host;
	if (gethostname(hostname, ARRAY_SIZE(hostname)) != 0 || strlen(hostname) == 0)
		return 0;

	if (host = gethostbyname(hostname)){
		for (size_t i = 0; host->h_addr_list[i]; ++i){
			uint32_t ip = ((uint32_t)*((uint32_t*)host->h_addr_list[i]));
			if (IS_PRIVATE_IP(ntohl(ip)) && !IS_VPN(ntohl(ip)))
				return ip;
		}
	}

	return 0;
}

