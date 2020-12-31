#if defined(OS_WINDOWS)
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>

/// @return 1-multicast address, 0-don't multicast address
int rtsp_addr_is_multicast(const char* ip)
{
    int r;
    struct addrinfo *ai;
    r = getaddrinfo(ip, 0, NULL, &ai);
    if (0 != r)
        return 0;

    if (AF_INET == ai->ai_addr->sa_family)
    {
        const struct sockaddr_in* in = (const struct sockaddr_in*)ai->ai_addr;
        assert(sizeof(struct sockaddr_in) == ai->ai_addrlen);
        r = (ntohl(in->sin_addr.s_addr) & 0xf0000000) == 0xe0000000 ? 1 : 0;
    }
    else if (AF_INET6 == ai->ai_addr->sa_family)
    {
        const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)ai->ai_addr;
        assert(sizeof(struct sockaddr_in6) == ai->ai_addrlen);
        r = in6->sin6_addr.s6_addr[0] == 0xff ? 1 : 0;
    }
    else
    {
        assert(0);
        r = 0;
    }
    
    freeaddrinfo(ai);
    return r;
}
