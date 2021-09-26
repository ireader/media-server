#include "sockutil.h"
#include "rtp-dump.h"
#include "sys/system.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

void rtp_dump_replay_test(const char* file, const char* peer, int port)
{
	int r;
	uint8_t data[1500];
	uint32_t clock, clock0;
	uint64_t base = 0;
	struct rtpdump_t* dump;

	socket_init();
	socket_t udp = socket_udp_bind_ipv4(NULL, 0);

	socklen_t addrlen = 0;
	struct sockaddr_storage addr;
	assert(0 == socket_addr_from(&addr, &addrlen, peer, port));

	dump = rtpdump_open(file, 0);
	while (1)
	{
		r = rtpdump_read(dump, &clock, data, sizeof(data));
		if (r <= 0)
			break;

		assert(r >= 0);
		uint64_t now = system_clock();
		if (0 == base)
		{
			base = now;
			clock0 = clock;
		}
		else
		{
			if (now - base < clock - clock0)
			{
				uint64_t v = (uint64_t)(clock - clock0) - (now - base);
				if(v < 5000)
					system_sleep(v);
			}	
		}

		printf("rtpdump replay [%u] bytes: %d\n", clock, r);
		assert(r == socket_sendto(udp, data, r, 0, (sockaddr*)&addr, addrlen));
	}

	rtpdump_close(dump);
	socket_close(udp);
	socket_cleanup();
}
