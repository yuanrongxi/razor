#include "cf_platform.h"

#define RELAY_PORT		9200
#define MAX_UDP_SIZE	1500
static void echo_relay()
{
	su_addr addr;
	su_socket fd;
	int rc;
	uint8_t buf[MAX_UDP_SIZE];

	if (su_udp_create(NULL, RELAY_PORT, &fd) != 0){
		printf("bind udp port failed! port=%u\n", RELAY_PORT);
		return;
	}

	while (1){
		rc = su_udp_recv(fd, &addr, buf, MAX_UDP_SIZE, 20);
		if (rc > 0)
			su_udp_send(fd, &addr, buf, rc);
	}

	su_socket_destroy(fd);
}

static void peer_relay(const char* addr_str1, const char* addr_str2)
{
	su_addr addr, addr1, addr2;
	su_socket fd;
	int rc;
	uint8_t buf[MAX_UDP_SIZE];
	char ip[32];

	su_string_to_addr(&addr1, (char*)addr_str1);
	su_addr_to_string(&addr1, ip, 32);
	printf("peer1 addr = %s\n", ip);

	su_string_to_addr(&addr2, (char*)addr_str2);
	su_addr_to_string(&addr2, ip, 32);
	printf("peer2 addr = %s\n", ip);

	if (su_udp_create(NULL, RELAY_PORT, &fd) != 0){
		printf("bind udp port failed! port=%u\n", RELAY_PORT);
		return;
	}

	while (1){
		rc = su_udp_recv(fd, &addr, buf, MAX_UDP_SIZE, 20);
		if (rc > 0){
			if (su_addr_eq(&addr1, &addr) == 0)
				su_udp_send(fd, &addr2, buf, rc);
			else if (su_addr_eq(&addr2, &addr) == 0)
				su_udp_send(fd, &addr1, buf, rc);
		}
	}

	su_socket_destroy(fd);
}

/*这个是中转测试程序，用于模拟丢包和延迟用的，程序运行在linux下，配合sim_sender和sim_receiver进行测试，他们的地址设置到这里*/
int main(int argc, const char* argv[])
{
	su_platform_init();

	if (argc == 1)
		echo_relay();
	else if (argc == 3)
		peer_relay(argv[1], argv[2]);
	else{
		printf("run param:\n");
		printf("  peer			sim_relay [addr1] [addr2], addr=ip:port\n");		/*配对地址，这两个地址之间中转数据*/
		printf("  echo			sim_relay\n");
	}

	su_platform_uninit();

	return 0;
}



