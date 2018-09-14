#include "sockutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

void rtp_sender_test(const char* peer, int port, const char* rtpfile)
{
	uint8_t packet[64 * 1024];

	socket_init();
	socket_t udp = socket_udp_bind(NULL, 0);

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	socket_addr_from(&addr, &addrlen, peer, port);

	FILE* fp = fopen(rtpfile, "rb");

	uint8_t s2[2];
	while (2 == fread(s2, 1, 2, fp))
	{
		int size = (s2[0] << 8) | s2[1];
		assert(size < sizeof(packet));
		if (size != (int)fread(packet, 1, size, fp))
			break;

		int r = socket_sendto(udp, packet, size, 0, (struct sockaddr*)&addr, addrlen);
		assert(size == r);
	}

	fclose(fp);
	socket_close(udp);
	socket_cleanup();
}
