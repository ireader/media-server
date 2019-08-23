#include "rtp-socket.h"
#include "sockutil.h"
#include "sys/atomic.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static unsigned short s_base_port = 30000;
static unsigned short s_port_num = 30000;

static char s_multicast_ip[SOCKET_ADDRLEN];
static unsigned int s_multicast_num = 250;

#if defined(OS_LINUX)
static unsigned char s_port_map[(1<<(sizeof(unsigned short)*8))/8];

static inline void system_list_port()
{
	FILE* fp;
	char line[256];
	unsigned int port;

	memset(s_port_map, 0, sizeof(s_port_map));

	fp = fopen("/proc/self/net/udp", "r");
	while(fgets(line, sizeof(line), fp))
	{
		// sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops
		// 16: 83331C73:007B 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 8869 2 ffff88007b9aaa40 0
		// 16: E92F900A:007B 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 8868 2 ffff88007b9aad80 0
		if(1 == sscanf(line, " %*d: %*p:%x", &port))
		{
			s_port_map[port/8] |= (1 << (port%8));
		}
	}
	fclose(fp);
}
#endif

int rtp_socket_create2(const struct sockaddr* addr, socket_t rtp[2], unsigned short port[2])
{
	unsigned short i;
	socket_t sock[2];
	struct sockaddr_storage ss;
	assert(0 == s_base_port % 2);
	srand((unsigned int)time(NULL));
	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, addr, socket_addr_len(addr));

	do
	{
		i = rand() % 30000;
		i = i / 2 * 2 + s_base_port;

		socket_addr_setport((struct sockaddr*)&ss, socket_addr_len(addr), i);
		sock[0] = socket_udp_bind_addr(addr, 0, 0);
		if (socket_invalid == sock[0])
			continue;

		socket_addr_setport((struct sockaddr*)&ss, socket_addr_len(addr), i+1);
		sock[1] = socket_udp_bind_addr(addr, 0, 0);
		if (socket_invalid == sock[1])
		{
			socket_close(sock[0]);
			continue;
		}

		rtp[0] = sock[0];
		rtp[1] = sock[1];
		port[0] = i;
		port[1] = i + 1;
		return 0;

	} while (socket_invalid == sock[0] || socket_invalid == sock[1]);

	return -1;
}

int rtp_socket_create(const char* ip, socket_t rtp[2], unsigned short port[2])
{
	socklen_t len;
	struct sockaddr_storage ss;

	if (0 != socket_addr_from(&ss, &len, ip, 0 /* placeholder */ ))
		return -1;
	return rtp_socket_create2((struct sockaddr*)&ss, rtp, port);
}

void rtp_socket_set_port_range(unsigned short base, unsigned short num)
{
	s_base_port = base;
	s_port_num = num;
}

void rtp_socket_get_port_range(unsigned short *base, unsigned short *num)
{
	*base = s_base_port;
	*num = s_port_num;
}

void rtp_socket_set_multicast_range(const char* multicast, unsigned int num)
{
	memset(s_multicast_ip, 0, sizeof(s_multicast_ip));
	snprintf(s_multicast_ip, sizeof(s_multicast_ip), "%s", multicast);
	s_multicast_num = num;
}

void rtp_socket_get_multicast_range(char multicast[SOCKET_ADDRLEN], unsigned int *num)
{
	snprintf(multicast, SOCKET_ADDRLEN, "%s", s_multicast_ip);
	*num = s_multicast_num;
}
