#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/if.h>
#else
#include <net/if.h>
#endif
#include <fcntl.h>
#include <dirent.h>

#include <netdb.h>

#include "cf_platform.h"

int do_fork;

int su_platform_init()
{
	return 0;
}

int su_platform_uninit()
{
	return 0;
}

su_thread su_create_thread(char *name, su_thread_fun func, void *data)
{
	pthread_t thread;
	if (pthread_create(&thread, NULL, func, data) == 0)
		return (su_thread)thread;

	pthread_detach(thread);
	return NULL;
}

void su_destroy_thread(su_thread thread)
{
	pthread_join((pthread_t)thread, NULL);
}

su_mutex su_create_mutex()
{
	pthread_mutex_t *mutex;
	mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutex, NULL);
	return (su_mutex)mutex;
}

void su_mutex_lock(su_mutex mutex)
{
	pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int su_mutex_trylock(su_mutex mutex)
{
	return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

void su_mutex_unlock(su_mutex mutex)
{
	pthread_mutex_unlock((pthread_mutex_t *)mutex);
}
void su_destroy_mutex(su_mutex mutex)
{
	pthread_mutex_destroy((pthread_mutex_t *)mutex);
	free(mutex);
}

typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} su_cond_t;

su_cond su_create_cond()
{
	su_cond_t *cond = (su_cond_t *)malloc(sizeof(su_cond_t));
	pthread_cond_init(&(cond->cond), NULL);
	pthread_mutex_init(&(cond->mutex), NULL);

	return (su_cond)cond;
}

void su_cond_wait(su_cond cond)
{
	su_cond_t *c = (su_cond_t *)cond;
	pthread_mutex_lock(&(c->mutex));
	pthread_cond_wait(&(c->cond), &(c->mutex));
	pthread_mutex_unlock(&(c->mutex));
}

void su_cond_signal(su_cond cond)
{
	su_cond_t *c = (su_cond_t *)cond;
	pthread_mutex_lock(&(c->mutex));
	pthread_cond_broadcast(&(c->cond));
	pthread_mutex_unlock(&(c->mutex));
}

void su_destroy_cond(su_cond cond)
{
	su_cond_t *c = (su_cond_t *)cond;
	pthread_cond_destroy(&(c->cond));
	pthread_mutex_destroy(&(c->mutex));

	free(cond);
}

int64_t su_get_sys_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec + (int64_t)tv.tv_sec * 1000 * 1000;
}
int su_udp_create(const char *ip, uint16_t port, su_socket *fd)
{
#ifdef SU_SERVER
	int buf_size = 1024 * 1024;
#else
	int buf_size = 128 * 1024;
#endif

	int len = sizeof(int);

	int s;
	struct sockaddr_in addr;
	s = socket(AF_INET, SOCK_DGRAM, 0);

	/*set server's socket buffer!*/
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, &buf_size, len);
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &buf_size, len);

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

#ifdef SU_SERVER
	if (ip == NULL || strlen(ip) == 0)
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		addr.sin_addr.s_addr = inet_addr(ip);

	su_socket_noblocking(s);
#else
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

	if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
		return -1;

	*fd = s;

	return 0;
}

int su_tcp_create(su_socket *fd)
{
	int buf_size = 16 * 1024;
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return -1;

	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int));

	*fd = s;

	return 0;
}

int su_tcp_listen_create(uint16_t port, su_socket *fd)
{
	int s;
	int flags, buf_size;
	struct sockaddr_in addr;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		return -1;

	if ((flags = fcntl(s, F_GETFL, 0)) < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		close(s);
		return -1;
	}

	buf_size = 16 * 1024;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (void *)&buf_size, sizeof(int32_t));
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&buf_size, sizeof(int32_t));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
	{
		printf("bind port failed, port = %u\n", port);
		close(s);
		return -1;
	}

	if (listen(s, 4096) == -1)
	{
		printf("socket listen failed, err = %d\n", errno);
		close(s);
		return -1;
	}

	*fd = s;

	return 0;
}

int su_socket_noblocking(int fd)
{
	int val = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, val | O_NONBLOCK);

	return 0;
}

int32_t su_udp_send(su_socket fd, su_addr *addr, void *buf, uint32_t len)
{
	return sendto(fd, buf, len, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

int32_t su_udp_recv(su_socket fd, su_addr *addr, void *buf, uint32_t len, uint32_t ms)
{
	int32_t ret = -1;
	if (ms > 0)
	{
		fd_set fds;
		struct timeval timeout = {0, ms * 1000};

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

	socklen_t addr_len = sizeof(struct sockaddr_in);
	ret = recvfrom(fd, (char *)buf, len, 0, (struct sockaddr *)addr, &addr_len);
	if (ret <= 0)
		return -1;

	return ret;
}

void su_socket_destroy(su_socket fd)
{
	close(fd);
}

void su_set_addr(su_addr *addr, const char *ip, uint16_t port)
{
	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(ip);
}
void su_set_addr2(su_addr *addr, uint32_t ip, uint16_t port)
{
	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = (in_addr_t)(ip);
}
void su_addr_to_addr(su_addr *src, su_addr *dst)
{
	dst->sin_family = AF_INET;
	dst->sin_port = src->sin_port;
	dst->sin_addr.s_addr = src->sin_addr.s_addr;
}

char *su_addr_to_string(su_addr *addr, char *str, int len)
{
	if (len < 24 || addr == NULL || str == NULL)
		return NULL;

	inet_ntop(addr->sin_family, &(addr->sin_addr), str, len);
	sprintf(str, "%s:%d", str, ntohs(addr->sin_port));

	return str;
}

void su_string_to_addr(su_addr *addr, char *str)
{
	char *pos, ip[IP_SIZE] = {0};
	uint16_t port, i;

	if (addr == NULL || str == NULL)
		return;

	i = 0;
	pos = str;
	do
	{
		ip[i++] = *pos++;
	} while (*pos != ':' && pos != '\0' && i < IP_SIZE - 1);

	if (*pos == ':')
		pos++;

	port = atoi(pos);
	su_set_addr(addr, ip, port);
}

void su_addrstr_to_iport(char *in, char *ip, int *pport)
{
	char *p = strrchr(in, ':');
	if (p)
	{
		int len = p - in;
		memcpy(ip, in, len);
		ip[len] = '\0';

		uint16_t port = atoi(p + 1);
		*pport = port;
	}
	else
	{
		*pport = -1;
	}
}

char *su_addr_to_iport(su_addr *addr, char *str, int len, uint16_t *port)
{
	if (len < 24 || addr == NULL || str == NULL)
		return NULL;

	inet_ntop(addr->sin_family, &(addr->sin_addr), str, len);
	*port = ntohs(addr->sin_port);
	return str;
}

int su_addr_cmp(su_addr *src, su_addr *dst)
{
	if (src->sin_addr.s_addr > dst->sin_addr.s_addr)
		return 1;
	else if (src->sin_addr.s_addr < dst->sin_addr.s_addr)
		return -1;
	else if (src->sin_port > dst->sin_port)
		return 1;
	else if (src->sin_port < dst->sin_port)
		return -1;
	else
		return 0;
}

int su_addr_eq(su_addr *src, su_addr *dst)
{
	if (src->sin_addr.s_addr == dst->sin_addr.s_addr && src->sin_port == dst->sin_port)
		return 0;

	return -1;
}

void su_sleep(uint64_t seconds, uint64_t micro_seconds)
{
	struct timeval t;

	t.tv_sec = (time_t)(seconds + micro_seconds / 1000000);
	t.tv_usec = (suseconds_t)(micro_seconds % 1000000);

	select(0, NULL, NULL, NULL, &t);
}

#define ARRAY_SIZE(x) (static_cast<int>((sizeof(x) / sizeof(x[0]))))

#define IS_PRIVATE_IP(ip) \
	((((ip) >> 24) == 127) || (((ip) >> 24) == 10) || (((ip) >> 20) == ((172 << 4) | 1)) || (((ip) >> 16) == ((192 << 8) | 168)))

#define IS_VPN(ip) ((((ip) >> 16) & 0x00ff) > 250)

#define MAXINTERFACES 16

#ifdef SU_SERVER

uint32_t su_get_local_ip()
{
	return 0;
}

#else

uint32_t su_get_local_ip()
{
	long ip;
	int fd, intrface, retn = 0;
	struct ifreq buf[MAXINTERFACES];
	struct ifconf ifc;
	ip = -1;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		ifc.ifc_len = sizeof buf;
		ifc.ifc_buf = (caddr_t)buf;

		if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
		{
			intrface = ifc.ifc_len / sizeof(struct ifreq);

			while (intrface-- > 0)
			{
				if (!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface])))
				{
					ip = inet_addr(inet_ntoa(((struct sockaddr_in *)(&buf[intrface].ifr_addr))->sin_addr));
					if (IS_PRIVATE_IP(ntohl(ip)) && !IS_VPN(ntohl(ip)))
						break;
				}
			}
		}
		close(fd);
	}
	return (uint32_t)(ip & 0xffffffff);
}
#endif


