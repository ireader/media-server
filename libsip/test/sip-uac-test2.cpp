#include <map>
#include <memory>
#include <string>
#include "sockutil.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/pollfd.h"
#include "port/network.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "rtp.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include "stun-proto.h"
#include "ice-agent.h"
#include "stun-agent.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "time64.h"
#include "sdp.h"
#include "md5.h"
#include "aio-timeout.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "rtsp-media.h"
#include "../test/rtp-sender.h"
#include "../test/rtp-socket.h"
#include "../test/media/mp4-file-reader.h"

#define N_FOUNDATION 8

#define SIP_USR "tao3"
#define SIP_PWD "123456"
#define SIP_HOST "sip.linphone.org"
#define SIP_FROM "sip:tao3@sip.linphone.org"
#define SIP_PEER "sip:ch.magus@sip.linphone.org"
#define SIP_EXPIRED 60
#define LOCAL_HOST "10.95.50.79"
#define STUN_SERVER "stun.linphone.org"

extern "C" void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding);

struct sip_uac_test2_session_t
{
    char buffer[2 * 1024 * 1024];
	struct rtsp_media_t medias[3];
	int nmedia;
	std::string from;
	struct sip_uas_transaction_t* t;

	struct av_media_t
	{
		struct rtsp_media_t* m;
		int stream; // stream index, base 0
		int fmt;

		enum AVPACKET_CODEC_ID codec;
		void* decoder;
        
		// ice candidates
		socket_t udp[N_FOUNDATION][2];
		struct sockaddr_storage local[N_FOUNDATION][2];
		int ncandidates;
		int candused;

        struct sockaddr_storage remote[2];
		socklen_t addrlen[2];
        struct rtp_sender_t sender;
        time64_t clock;
        int track;
        FILE* fp;
        
        union
        {
            struct mpeg4_aac_t aac;
            struct mpeg4_avc_t avc;
            struct mpeg4_hevc_t hevc;
        } u;
	} audio, video;

	struct ice_agent_t* ice;

    pthread_t th;
	bool running;
    std::shared_ptr<AVPacketQueue> pkts;
	std::shared_ptr<VodFileSource> source;
};

struct sip_uac_test2_t
{
	socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;
    bool running;

	struct sip_agent_t* sip;
	struct sip_transport_t transport;
	struct http_header_www_authenticate_t auth;
	int nonce_count;
    
	ThreadLocker locker;
	typedef std::map<std::string, std::shared_ptr<sip_uac_test2_session_t> > TSessions;
	TSessions sessions;
};

static int sip_uac_transport_via(void* transport, const char* destination, char protocol[16], char local[128], char dns[128])
{
	int r;
	char ip[65];
	u_short port;
	struct uri_t* uri;

	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)transport;

	// rfc3263 4.1 Selecting a Transport Protocol
	// 1. If the URI specifies a transport protocol in the transport parameter,
	//    that transport protocol SHOULD be used. Otherwise, if no transport 
	//    protocol is specified, but the TARGET(maddr) is a numeric IP address, 
	//    the client SHOULD use UDP for a SIP URI, and TCP for a SIPS URI.
	// 2. if no transport protocol is specified, and the TARGET is not numeric, 
	//    but an explicit port is provided, the client SHOULD use UDP for a SIP URI, 
	//    and TCP for a SIPS URI
	// 3. Otherwise, if no transport protocol or port is specified, and the target 
	//    is not a numeric IP address, the client SHOULD perform a NAPTR query for 
	//    the domain in the URI.

	// The client SHOULD try the first record. If an attempt should fail, based on 
	// the definition of failure in Section 4.3, the next SHOULD be tried, and if 
	// that should fail, the next SHOULD be tried, and so on.

	test->addrlen = sizeof(test->addr);
	memset(&test->addr, 0, sizeof(test->addr));
	strcpy(protocol, "UDP");

	uri = uri_parse(destination, strlen(destination));
	if (!uri)
		return -1; // invalid uri

	// rfc3263 4-Client Usage (p5)
	// once a SIP server has successfully been contacted (success is defined below), 
	// all retransmissions of the SIP request and the ACK for non-2xx SIP responses 
	// to INVITE MUST be sent to the same host.
	// Furthermore, a CANCEL for a particular SIP request MUST be sent to the same 
	// SIP server that the SIP request was delivered to.

	// TODO: sips port
	r = socket_addr_from(&test->addr, &test->addrlen, uri->host, uri->port ? uri->port : SIP_PORT);
	if (0 == r)
	{
		socket_addr_to((struct sockaddr*)&test->addr, test->addrlen, ip, &port);
		socket_getname(test->udp, local, &port);
		r = ip_route_get(ip, local);
		if (0 == r)
		{
			dns[0] = 0;
			struct sockaddr_storage ss;
			socklen_t len = sizeof(ss);
			if (0 == socket_addr_from(&ss, &len, local, port))
				socket_addr_name((struct sockaddr*)&ss, len, dns, 128);

			if (SIP_PORT != port)
				snprintf(local + strlen(local), 128 - strlen(local), ":%hu", port);

			if (NULL == strchr(dns, '.'))
				snprintf(dns, 128, "%s", local); // don't have valid dns
		}
	}

	uri_free(uri);
	return r;
}

static int sip_uac_transport_send(void* transport, const void* data, size_t bytes)
{
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)transport;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n", (const char*)data);
	int r = socket_sendto(test->udp, data, bytes, 0, (struct sockaddr*)&test->addr, test->addrlen);
	return r == bytes ? 0 : -1;
}

static int sip_uas_transport_send(void* param, const struct cstring_t* url, const void* data, int bytes)
{
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n\n", (const char*)data);
	int r = socket_sendto(test->udp, data, bytes, 0, (struct sockaddr*)&test->addr, test->addrlen);
	return r == bytes ? 0 : -1;
}

static int sdp_media_negotiate(const struct rtsp_media_t* m, sip_uac_test2_session_t::av_media_t* av)
{
	int i, j;
    const int payloads[] = { RTP_PAYLOAD_PCMA, RTP_PAYLOAD_PCMU };
    const char* payloads2[] = { "H264", "H265", "MP4V-ES", "MP4A-LATM", "MPEG4-GENERIC", "opus" };
    
	assert(0 == strcasecmp("IP4", m->addrtype) || 0 == strcasecmp("IP6", m->addrtype));
	for (i = 0; i < m->avformat_count; i++)
	{
		if(m->avformats[i].fmt < 96)
        {
            for(j = 0; j < sizeof(payloads)/sizeof(payloads[0]); j++)
            {
                if(payloads[j] == m->avformats[i].fmt)
                    return i;
            }
        }
        else
        {
            for(j = 0; j < sizeof(payloads2)/sizeof(payloads2[0]); j++)
            {
                if(0 == strcmp(m->avformats[i].encoding, payloads2[j]))
                    return i;
            }
        }
	}
	return -1;
}

static void rtp_packet_onrecv(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    sip_uac_test2_session_t::av_media_t* av = (sip_uac_test2_session_t::av_media_t*)param;
    fwrite(packet, 1, bytes, av->fp);
}

static int rtp_packet_send(void* param, const void *packet, int bytes)
{
    sip_uac_test2_session_t::av_media_t* av = (sip_uac_test2_session_t::av_media_t*)param;
    return socket_sendto(av->udp[av->candused][0], packet, bytes, 0, (struct sockaddr*)&av->remote[0], av->addrlen[0]);
}

static void mp4_onvideo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
    if(!s->video.m)
        return; // ignore video

	s->video.track = track;
	s->video.fp = fopen("sipvideo.h264", "wb");

	char ip[SOCKET_ADDRLEN];
	u_short port;
	struct ice_candidate_t c;
	assert(0 == ice_agent_get_candidate(s->ice, s->video.stream, 1, &c));
	struct sockaddr* addr = (struct sockaddr*)&c.addr;
	socket_addr_to(addr, socket_addr_len(addr), ip, &port);

	if (MOV_OBJECT_H264 == object)
	{
		s->video.codec = AVCODEC_VIDEO_H264;
        mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s->video.u.avc);
        rtp_sender_init_video(&s->video.sender, port, s->video.m->avformats[s->video.fmt].fmt, s->video.m->avformats[s->video.fmt].encoding, width, height, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		s->video.codec = AVCODEC_VIDEO_H265;
        mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s->video.u.hevc);
        rtp_sender_init_video(&s->video.sender, port, s->video.m->avformats[s->video.fmt].fmt, s->video.m->avformats[s->video.fmt].encoding,width, height, extra, bytes);
	}
	else if (MOV_OBJECT_MP4V == object)
	{
		s->video.codec = AVCODEC_VIDEO_MPEG4;
        rtp_sender_init_video(&s->video.sender, port, s->video.m->avformats[s->video.fmt].fmt, s->video.m->avformats[s->video.fmt].encoding,width, height, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	int n = strlen((char*)s->video.sender.buffer);
	snprintf((char*)s->video.sender.buffer + n, sizeof(s->video.sender.buffer) - n, "c=IN %s %s\r\n", AF_INET6 == addr->sa_family ? "IP6" : "IP4", ip);
}

static void mp4_onaudio(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
    if(!s->audio.m)
        return;

	s->audio.track = track;
	s->audio.fp = fopen("sipaudio.pcm", "wb");

	char ip[SOCKET_ADDRLEN];
	u_short port;
	struct ice_candidate_t c;
	assert(0 == ice_agent_get_candidate(s->ice, s->audio.stream, 1, &c));
	struct sockaddr* addr = (struct sockaddr*)&c.addr;
	socket_addr_to(addr, socket_addr_len(addr), ip, &port);

	if (MOV_OBJECT_AAC == object || MOV_OBJECT_AAC_LOW == object)
	{
        s->audio.codec = AVCODEC_AUDIO_AAC;
        mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s->audio.u.aac);
        rtp_sender_init_audio(&s->audio.sender, port, s->audio.m->avformats[s->audio.fmt].fmt, s->audio.m->avformats[s->audio.fmt].encoding,channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
	else if (MOV_OBJECT_OPUS == object)
	{
        s->audio.codec = AVCODEC_AUDIO_OPUS;
        rtp_sender_init_audio(&s->audio.sender, port, s->audio.m->avformats[s->audio.fmt].fmt, s->audio.m->avformats[s->audio.fmt].encoding,channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
        s->audio.codec = AVCODEC_AUDIO_PCM;
        rtp_sender_init_audio(&s->audio.sender, port, s->audio.m->avformats[s->audio.fmt].fmt, s->audio.m->avformats[s->audio.fmt].encoding,channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
    else if (MOV_OBJECT_G711a == object)
    {
        s->audio.codec = AVCODEC_AUDIO_PCM;
        rtp_sender_init_audio(&s->audio.sender, port, s->audio.m->avformats[s->audio.fmt].fmt, s->audio.m->avformats[s->audio.fmt].encoding,channel_count, bit_per_sample, sample_rate, extra, bytes);
    }
    else
	{
		assert(0);
		return;
	}

	int n = strlen((char*)s->audio.sender.buffer);
	snprintf((char*)s->audio.sender.buffer + n, sizeof(s->audio.sender.buffer) - n, "c=IN %s %s\r\n", AF_INET6 == addr->sa_family ? "IP6" : "IP4", ip);
}

static void ice_agent_test_ondata(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;
	/// TODO: check data format(stun/turn)
	ice_agent_input(s->ice, protocol, local, remote, data, bytes);
}

static int ice_agent_test_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;
	assert(STUN_PROTOCOL_UDP == protocol);
	for (int stream = 0; stream < 2; stream++)
	{
		sip_uac_test2_session_t::av_media_t* av = 0 == stream ? &s->audio : &s->video;
		for (int foundation = 0; foundation < av->ncandidates; foundation++)
		{
			for (int component = 0; component < 2; component++)
			{
				if (0 == socket_addr_compare((const struct sockaddr*)&av->local[foundation][component], local))
				{
					int r = socket_sendto(av->udp[foundation][component], data, bytes, 0, remote, socket_addr_len(remote));
					assert(r == bytes || socket_geterror() == ENETUNREACH || socket_geterror() == 10051/*WSAENETUNREACH*/);
					return r == bytes ? 0 : socket_geterror();
				}
			}
		}
	}

	return -1;
}

static void ice_agent_test_onconnected(void* param, int code)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;
}

// candidate-attribute = "candidate" ":" foundation SP component-id SP transport SP priority SP connection-address SP port SP cand-type [SP rel-addr] [SP rel-port] *(SP extension-att-name SP extension-att-value)
// a=candidate:1 1 UDP 2130706431 $L-PRIV-1.IP $L-PRIV-1.PORT typ host
// a=candidate:2 1 UDP 1694498815 $NAT-PUB-1.IP $NAT-PUB-1.PORT typ srflx raddr $L-PRIV-1.IP rport $L-PRIV-1.PORT
static int ice_candidate_attribute(const struct ice_candidate_t* c, char* buf, int len)
{
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS", };
	char addr[SOCKET_ADDRLEN], raddr[SOCKET_ADDRLEN];
	u_short addrport, raddrport;
	const struct sockaddr* connaddr = (const struct sockaddr*)&c->addr;
	const struct sockaddr* realaddr = (const struct sockaddr*)ice_candidate_realaddr(c);
	assert(c->protocol <= 0 && c->protocol < sizeof(s_transport) / sizeof(s_transport[0]));
	socket_addr_to(connaddr, socket_addr_len(connaddr), addr, &addrport);
	socket_addr_to(realaddr, socket_addr_len(realaddr), raddr, &raddrport);
	if (ICE_CANDIDATE_HOST == c->type)
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ host", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport);
	else
		return snprintf(buf, len, "%s %hu %s %u %s %hu typ %s raddr %s rport %hu", c->foundation, c->component, s_transport[c->protocol], c->priority, addr, addrport, ice_candidate_typename(c), raddr, raddrport);
}

static void sip_ice_gather_local_candidates(void* param, const char* mac, const char* name, int /*dhcp*/, const char* ip, const char* netmask, const char* gateway)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;
	if (NULL == ip || 0 == *ip || 0 == strcmp("0.0.0.0", ip))
		return; // ignore

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, STUN_SERVER, STUN_PORT));

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	socket_addr_from(&addr, &addrlen, ip, 0);

	for (int stream = 0; stream < 2; stream++)
	{
		unsigned short port[2];
		sip_uac_test2_session_t::av_media_t* av = 0 == stream ? &s->audio : &s->video;
		if (NULL == av->m || av->ncandidates + 1 >= sizeof(av->udp) / sizeof(av->udp[0]))
			continue;
		rtp_socket_create2((struct sockaddr*)&addr, av->udp[av->ncandidates], port);

		for (int component = 0; component < 2; component++)
		{
			struct ice_candidate_t c;
			memset(&c, 0, sizeof(c));
			c.type = ICE_CANDIDATE_HOST;
			c.stream = av->stream;
			c.protocol = STUN_PROTOCOL_UDP;
			c.component = component + 1;
			snprintf((char*)c.foundation, sizeof(c.foundation), "%d", av->ncandidates+1);
			getsockname(av->udp[av->ncandidates][component], (struct sockaddr*)&c.host, &addrlen);
			memcpy(&av->local[av->ncandidates][component], &c.host, sizeof(struct sockaddr_storage));
			ice_candidate_priority(&c);
			assert(0 == ice_agent_add_local_candidate(s->ice, &c));

			char attr[128] = { 0 };
			ice_candidate_attribute(&c, attr, sizeof(attr));
			printf("candidate: %s\n", attr);
		}

		++av->ncandidates;
	}
}

static int sip_ice_agent_oncandidate(const struct ice_candidate_t* c, const void* param)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	if (c->stream == s->audio.stream)
	{
		int n = strlen((char*)s->audio.sender.buffer);
		n += snprintf((char*)s->audio.sender.buffer + n, sizeof(s->audio.sender.buffer) - n, "%s", "a=candidate:");
		ice_candidate_attribute(c, (char*)s->audio.sender.buffer + n, sizeof(s->audio.sender.buffer) - n);
		n += snprintf((char*)s->audio.sender.buffer + n, sizeof(s->audio.sender.buffer) - n, "%s", "\n");
	}
	else if(c->stream == s->video.stream)
	{
		int n = strlen((char*)s->video.sender.buffer);
		n += snprintf((char*)s->video.sender.buffer + n, sizeof(s->video.sender.buffer) - n, "%s", "a=candidate:");
		ice_candidate_attribute(c, (char*)s->video.sender.buffer + n, sizeof(s->video.sender.buffer) - n);
		n += snprintf((char*)s->video.sender.buffer + n, sizeof(s->video.sender.buffer) - n, "%s", "\n");
	}
	else
	{
		assert(0);
	}
	return 0;
}

static void sip_ice_ongather(void* param, int code)
{
	const char* pattern = "v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Talk\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

	char reply[1024];
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	if (0 == code)
	{
		std::shared_ptr<MP4FileReader> reader(new MP4FileReader("d:\\video\\opus.mp4"));
		struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
		reader->GetInfo(&info, s);
		std::shared_ptr<VodFileSource> source(new VodFileSource(reader, s->pkts));
		s->source = source;
		ice_agent_list_local_candidate(s->ice, sip_ice_agent_oncandidate, s);

		sip_uas_add_header(s->t, "Content-Type", "application/sdp");
		sip_uas_add_header(s->t, "Contact", "sip:" SIP_USR "@" LOCAL_HOST);
		snprintf(reply, sizeof(reply), pattern, SIP_USR, LOCAL_HOST, LOCAL_HOST, s->audio.decoder ? (char*)s->audio.sender.buffer : "", s->video.decoder ? (char*)s->video.sender.buffer : "");
		assert(0 == sip_uas_reply(s->t, 200, reply, strlen(reply)));

		// start ice
		ice_agent_start(s->ice);
	}
	else
	{
		assert(0 == sip_uas_reply(s->t, 501, "Internal Server Error", 21));
	}
}

static int ice_candidate_transport(const char* transport)
{
	int i;
	static const char* s_transport[] = { "UDP", "TCP", "TLS", "DTLS" };
	static const int s_value[] = { STUN_PROTOCOL_UDP, STUN_PROTOCOL_TCP, STUN_PROTOCOL_TLS, STUN_PROTOCOL_DTLS };
	for (i = 0; i < sizeof(s_transport) / sizeof(s_transport[0]); i++)
	{
		if (0 == strcmp(s_transport[i], transport))
			return s_value[i];
	}
	return STUN_PROTOCOL_UDP;
}

static enum ice_candidate_type_t ice_candidate_name2type(const char* candtype)
{
	int i;
	static const char* s_candtype[] = { "host", "srflx", "prflx", "relay", };
	static const enum ice_candidate_type_t s_value[] = { ICE_CANDIDATE_HOST, ICE_CANDIDATE_SERVER_REFLEXIVE, ICE_CANDIDATE_PEER_REFLEXIVE, ICE_CANDIDATE_RELAYED };
	for (i = 0; i < sizeof(s_candtype) / sizeof(s_candtype[0]); i++)
	{
		if (0 == strcmp(s_candtype[i], candtype))
			return s_value[i];
	}
	return ICE_CANDIDATE_RELAYED;
}

static int sip_ice_add_remote_candidates(struct ice_agent_t* ice, int stream, const struct rtsp_media_t* m)
{
	int j;
	socklen_t len;
	struct ice_candidate_t c;

	for (j = 0; j < m->ice.ncandidate; j++)
	{
		memset(&c, 0, sizeof(c));
		c.stream = stream;
		c.priority = m->ice.candidates[j].priority;
		c.component = m->ice.candidates[j].component;
		c.protocol = ice_candidate_transport(m->ice.candidates[j].transport);
		c.type = ice_candidate_name2type(m->ice.candidates[j].candtype);
		snprintf(c.foundation, sizeof(c.foundation), "%s", m->ice.candidates[j].foundation);
		switch (c.type)
		{
		case ICE_CANDIDATE_HOST:
			socket_addr_from(&c.host, &len, m->ice.candidates[j].address, m->ice.candidates[j].port);
			break;
		case ICE_CANDIDATE_SERVER_REFLEXIVE:
		case ICE_CANDIDATE_PEER_REFLEXIVE:
			socket_addr_from(&c.addr, &len, m->ice.candidates[j].address, m->ice.candidates[j].port);
			socket_addr_from(&c.host, &len, m->ice.candidates[j].reladdr, m->ice.candidates[j].relport);
			break;
		case ICE_CANDIDATE_RELAYED:
			socket_addr_from(&c.addr, &len, m->ice.candidates[j].address, m->ice.candidates[j].port);
			socket_addr_from(&c.host, &len, m->ice.candidates[j].reladdr, m->ice.candidates[j].relport);
			break;
		default:
			assert(0);
		}

		assert(0 == ice_agent_add_remote_candidate(ice, &c));
	}

	return 0;
}

static int rtp_read(struct ice_agent_t* ice, struct sip_uac_test2_session_t::av_media_t* av, int i)
{
	int r;
	socklen_t len;
	struct sockaddr_storage ss;

	assert(0 == i % 2);
	len = sizeof(ss);
	r = socket_recvfrom(av->udp[i / 2][0], av->sender.buffer, sizeof(av->sender.buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;

	if (0 > ice_agent_input(ice, STUN_PROTOCOL_UDP, (struct sockaddr*)&av->local[i / 2][i % 2], (struct sockaddr*)&ss, av->sender.buffer, r))
	{
		assert(0 == socket_addr_compare((struct sockaddr*)&ss, (struct sockaddr*)&av->remote[0]));
		rtp_payload_decode_input(av->decoder, av->sender.buffer, r);
		rtp_onreceived(av->sender.rtp, av->sender.buffer, r);
	}
	return r;
}

static int rtcp_read(struct ice_agent_t* ice, struct sip_uac_test2_session_t::av_media_t* av, int i)
{
	int r;
	socklen_t len;
	struct sockaddr_storage ss;

	assert(1 == i % 2);
	len = sizeof(ss);
	r = socket_recvfrom(av->udp[i / 2][1], av->sender.buffer, sizeof(av->sender.buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;

	if (0 > ice_agent_input(ice, STUN_PROTOCOL_UDP, (struct sockaddr*)&av->local[i / 2][i % 2], (struct sockaddr*)&ss, av->sender.buffer, r))
	{
		assert(0 == socket_addr_compare((struct sockaddr*)&ss, (struct sockaddr*)&av->remote[1]));
		r = rtp_onreceived_rtcp(av->decoder, av->sender.buffer, r);
	}
	return r;
}

static int rtcp_report(struct sip_uac_test2_session_t::av_media_t* av, time64_t clock)
{
	int r;
	int interval = rtp_rtcp_interval(av->sender.rtp);
	if (av->clock + interval < clock)
	{
		r = rtp_rtcp_report(av->sender.rtp, av->sender.buffer, sizeof(av->sender.buffer));
		r = socket_sendto_by_time(av->udp[av->candused][1], av->sender.buffer, r, 0, (const sockaddr*)&av->remote[1], av->addrlen[1], 2000);
		av->clock = clock;
	}
	return 0;
}

static int STDCALL sip_work_thread(void* param)
{
	int r;
	time64_t clock;
	socket_t udp[N_FOUNDATION * 2 * 2];

	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	while (s->running)
	{
		clock = time64_now();

		// RTCP report
		if (s->audio.sender.rtp && s->audio.candused >= 0)
			rtcp_report(&s->audio, clock);
		if (s->video.sender.rtp && s->video.candused >= 0)
			rtcp_report(&s->video, clock);

		for (int i = 0; i < N_FOUNDATION * 2; i++)
			udp[i] = s->audio.udp[i / 2][i % 2];
		for (int i = 0; i < N_FOUNDATION * 2; i++)
			udp[N_FOUNDATION * 2 + i] = s->video.udp[i / 2][i % 2];

		r = socket_poll_read(udp, N_FOUNDATION * 2 * 2, 10);
		for (std::shared_ptr<avpacket_t> pkt(s->pkts->FrontWait(0), avpacket_release); pkt.get(); pkt.reset(s->pkts->FrontWait(0), avpacket_release))
		{
			if (pkt->stream == s->audio.track)
			{
				uint32_t timestamp = s->audio.sender.timestamp + (uint32_t)(pkt->pts * (s->audio.sender.frequency / 1000) /*kHz*/);
				rtp_payload_encode_input(s->audio.sender.encoder, pkt->data, pkt->size, timestamp);
				printf("send audio[%d] packet pts: %" PRId64 ", timestamp: %u\n", s->audio.sender.frequency, pkt->pts, timestamp);
			}
			else if (pkt->stream == s->video.track)
			{
				int n = h264_mp4toannexb(&s->video.u.avc, pkt->data, pkt->size, s->buffer, sizeof(s->buffer));
				uint32_t timestamp = s->video.sender.timestamp + (uint32_t)(pkt->pts * (s->video.sender.frequency / 1000) /*kHz*/);
				rtp_payload_encode_input(s->video.sender.encoder, s->buffer, n, timestamp);
				printf("send video[%d] packet pts: %" PRId64 ", timestamp: %u\n", s->video.sender.frequency, pkt->pts, timestamp);
			}
			else
			{
				assert(0);
			}

			s->pkts->Pop();
		}

		if (0 == r)
		{
			continue; // timeout
		}
		else if (r < 0)
		{
			system_sleep(10);
			continue;
			//return r; // error
		}
		else
		{
			for (int i = 0; i < N_FOUNDATION * 2; i++)
			{
				if (r & (1 << i))
				{
					0 == i % 2 ? rtp_read(s->ice, &s->audio, i) : rtcp_read(s->ice, &s->audio, i);
				}
			}
			for (int i = 0; i < N_FOUNDATION * 2; i++)
			{
				if (r & (1 << (i + N_FOUNDATION * 2)))
				{
					0 == i % 2 ? rtp_read(s->ice, &s->video, i) : rtcp_read(s->ice, &s->video, i);
				}
			}
		}
	}

	//size_t n = rtp_rtcp_bye(m->rtp, rtcp, sizeof(rtcp));
	s->source->Pause();
	return 0;
}

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
    sip_uac_test2_t* ctx = (sip_uac_test2_t*)param;

	const cstring_t* h = sip_message_get_header_by_name(req, "Content-Type");
	if (0 != cstrcasecmp(h, "application/sdp"))
	{
		assert(0);
		return NULL;
	}

	std::string from(req->from.uri.host.p, req->from.uri.host.n);
	std::shared_ptr<sip_uac_test2_session_t> s(new sip_uac_test2_session_t());
	s->pkts = std::shared_ptr<AVPacketQueue>(new AVPacketQueue(200));
	memset(&s->audio, 0, sizeof(s->audio));
	memset(&s->video, 0, sizeof(s->video));
	memset(s->audio.udp, -1, sizeof(s->audio.udp));
	memset(s->video.udp, -1, sizeof(s->video.udp));
	s->from = from;
	
	{
		AutoThreadLocker locker(ctx->locker);
		if (ctx->sessions.end() != ctx->sessions.find(from))
			return NULL; // ignore
		ctx->sessions.insert(std::make_pair(from, s));
	}

    struct ice_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = ice_agent_test_send;
	handler.ondata = ice_agent_test_ondata;
	handler.ongather = sip_ice_ongather;
	handler.onconnected = ice_agent_test_onconnected;
	s->ice = ice_agent_create(0, &handler, s.get());
	ice_agent_set_local_auth(s->ice, SIP_USR, SIP_PWD);

	s->nmedia = rtsp_media_sdp((const char*)data, s->medias, sizeof(s->medias) / sizeof(s->medias[0]));
	for (int i = 0; i < s->nmedia; i++)
	{
		struct rtsp_media_t* m = s->medias + i;
		sip_ice_add_remote_candidates(s->ice, i, m);

		if (0 == strcmp("audio", m->media))
		{
			int j = sdp_media_negotiate(m, &s->audio);
			if(-1 == j)
				continue;

			struct rtp_payload_t handler;
			handler.alloc = NULL;
			handler.free = NULL;
			handler.packet = rtp_packet_onrecv;
			s->audio.decoder = rtp_payload_decode_create(m->avformats[j].fmt, m->avformats[j].encoding, &handler, &s->audio);
			if (NULL == s->audio.decoder)
			{
				assert(0);
				continue; // ignore
			}

			s->audio.m = m;
			s->audio.fmt = j;
			s->audio.stream = i;
			s->audio.candused = -1;
			s->audio.ncandidates = 0;
			s->audio.sender.send = rtp_packet_send;
            s->audio.sender.param = &s->audio;
            socket_addr_from(&s->audio.remote[0], &s->audio.addrlen[0], m->address, m->port[0]);
			socket_addr_from(&s->audio.remote[1], &s->audio.addrlen[1], m->address, m->port[1]);
		}
		else if (0 == strcmp("video", m->media))
		{
			int j = sdp_media_negotiate(m, &s->video);
			if (-1 == j)
				continue;

            struct rtp_payload_t handler;
            handler.alloc = NULL;
            handler.free = NULL;
            handler.packet = rtp_packet_onrecv;
            s->video.decoder = rtp_payload_decode_create(m->avformats[j].fmt, m->avformats[j].encoding, &handler, &s->video);
			if (NULL == s->video.decoder)
			{
				assert(0);
				continue; // ignore
			}
                
			s->video.m = m;
			s->video.fmt = j;
			s->video.stream = i;
			s->video.candused = -1;
			s->video.ncandidates = 0;
			s->video.sender.send = rtp_packet_send;
            s->video.sender.param = &s->video;
            socket_addr_from(&s->video.remote[0], &s->video.addrlen[0], m->address, m->port[0]);
			socket_addr_from(&s->video.remote[1], &s->video.addrlen[1], m->address, m->port[1]);
		}
	}

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, STUN_SERVER, STUN_PORT));
	network_getip(sip_ice_gather_local_candidates, s.get());

	s->t = t;
	ice_agent_set_remote_auth(s->ice, s->medias[0].ice.ufrag, s->medias[0].ice.pwd);
	ice_agent_gather(s->ice, (struct sockaddr*)&stun, 0, 2000);

//      std::shared_ptr<AVPacketQueue> pkts(new AVPacketQueue(200));
//      std::shared_ptr<MP4FileReader> reader(new MP4FileReader("/Users/ireader/video/opus.mp4"));
//      struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio};
//      reader->GetInfo(&info, s.get());
//      std::shared_ptr<VodFileSource> source(new VodFileSource(reader, pkts));
//      
//      s->pkts = pkts;
	//s->source = source;

	//sip_uas_add_header(t, "Content-Type", "application/sdp");
	//sip_uas_add_header(t, "Contact", "sip:" SIP_USR "@" LOCAL_HOST);
//      snprintf(reply, sizeof(reply), pattern, SIP_USR, LOCAL_HOST, LOCAL_HOST, s->audio.decoder?(char*)s->audio.sender.buffer:"", s->video.decoder?(char*)s->video.sender.buffer:"");
	//assert(0 == sip_uas_reply(t, 200, reply, strlen(reply)));
	s->running = true;
	thread_create(&s->th, sip_work_thread, s.get());
    return s.get();
}

/// @param[in] code 0-ok, other-sip status code
/// @return 0-ok, other-error
static void sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	struct sip_uac_test2_t* ctx = (struct sip_uac_test2_t*)param; assert(ctx);
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)session; assert(s);
	printf("sip_uas_onack[%p]: %d\n", s, code);

	if (200 <= code && code < 300)
	{
		if(s->source.get())
			s->source->Play();
	}
	else
	{
		s->running = false;
		if (s->source.get())
			s->source->Pause();
		thread_destroy(s->th);

		AutoThreadLocker locker(ctx->locker);
		ctx->sessions.erase(s->from);
	}
}

/// on terminating a session(dialog)
static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_uac_test2_t* ctx = (struct sip_uac_test2_t*)param;
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)session;

	s->running = false;
	s->source->Pause();
	thread_destroy(s->th);

	sip_uac_test2_t::TSessions::iterator it;
	{
		AutoThreadLocker locker(ctx->locker);
		it = ctx->sessions.find(s->from);
		if (it == ctx->sessions.end())
			return sip_uas_reply(t, 481, NULL, 0);

		ctx->sessions.erase(it);
	}

	return sip_uas_reply(t, 200, NULL, 0);
}

/// cancel a transaction(should be an invite transaction)
static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

/// @param[in] expires in seconds
static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

static int sip_uas_onrequest(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

static int sip_uac_oninvited(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
    return 0;
}

static void sip_uac_invite_test(struct sip_uac_test2_t *ctx)
{
	const char* pattern = "v=0\n"
		"o=- 0 0 IN IP4 %s\n"
		"s=Play\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

    char buffer[1024];
    struct sip_uac_transaction_t* t;
    t = sip_uac_invite(ctx->sip, SIP_FROM, SIP_PEER, sip_uac_oninvited, ctx);
    ++ctx->auth.nc;
    snprintf(ctx->auth.uri, sizeof(ctx->auth.uri), "%s", SIP_PEER);
    snprintf(ctx->auth.username, sizeof(ctx->auth.username), "%s", SIP_USR);
    http_header_auth(&ctx->auth, SIP_PWD, "INVITE", NULL, 0, buffer, sizeof(buffer));
    sip_uac_add_header(t, "Proxy-Authorization", buffer);
	sip_uac_add_header(t, "Content-Type", "application/sdp");
	sip_uac_add_header(t, "Contact", "sip:" SIP_USR "@" LOCAL_HOST);

	std::shared_ptr<sip_uac_test2_session_t> s(new sip_uac_test2_session_t());
	s->pkts = std::shared_ptr<AVPacketQueue>(new AVPacketQueue(200));
	memset(&s->audio, 0, sizeof(s->audio));
	memset(&s->video, 0, sizeof(s->video));
	memset(s->audio.udp, -1, sizeof(s->audio.udp));
	memset(s->video.udp, -1, sizeof(s->video.udp));

	{
		AutoThreadLocker locker(ctx->locker);
		if (ctx->sessions.end() != ctx->sessions.find(SIP_FROM))
			return; // ignore
		ctx->sessions.insert(std::make_pair(SIP_FROM, s));
	}

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, STUN_SERVER, STUN_PORT));
	network_getip(sip_ice_gather_local_candidates, s.get());

	std::shared_ptr<MP4FileReader> reader(new MP4FileReader("d:\\video\\opus.mp4"));
	struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
	reader->GetInfo(&info, s.get());
	std::shared_ptr<VodFileSource> source(new VodFileSource(reader, s->pkts));
	s->source = source;
	ice_agent_list_local_candidate(s->ice, sip_ice_agent_oncandidate, s.get());

	int n = snprintf(buffer, sizeof(buffer), pattern, SIP_USR, LOCAL_HOST, LOCAL_HOST, s->audio.decoder ? (char*)s->audio.sender.buffer : "", s->video.decoder ? (char*)s->video.sender.buffer : "");
	//assert(0 == sip_uac_send(t, buffer, n, &test->transport, test));
}

static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	char buffer[1024];
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	if (200 <= code && code < 300)
	{
		printf("Register OK\n");
		sip_uac_invite_test(test);
	}
	else if (401 == code)
	{
		// https://blog.csdn.net/yunlianglinfeng/article/details/81109380
		// http://www.voidcn.com/article/p-oqqbqgvd-bgn.html
		const cstring_t* h;
		t = sip_uac_register(test->sip, SIP_FROM, SIP_PEER, SIP_EXPIRED, sip_uac_onregister, param);
		h = sip_message_get_header_by_name(reply, "Call-ID");
		sip_uac_add_header(t, "Call-ID", h->p); // All registrations from a UAC SHOULD use the same Call-ID
		h = sip_message_get_header_by_name(reply, "CSeq");
		snprintf(buffer, sizeof(buffer), "%u REGISTER", atoi(h->p) + 1);
		sip_uac_add_header(t, "CSeq", buffer); // A UA MUST increment the CSeq value by one for each REGISTER request with the same Call-ID

		// Unauthorized
		memset(&test->auth, 0, sizeof(test->auth));
		h = sip_message_get_header_by_name(reply, "WWW-Authenticate");
		assert(0 == http_header_www_authenticate(h->p, &test->auth));
		test->nonce_count = 0;
		switch (test->auth.scheme)
		{
		case HTTP_AUTHENTICATION_DIGEST:
			++test->auth.nc;
			snprintf(test->auth.uri, sizeof(test->auth.uri), "sip:%s", SIP_HOST);
			snprintf(test->auth.username, sizeof(test->auth.username), "%s", SIP_USR);
			http_header_auth(&test->auth, SIP_PWD, "REGISTER", NULL, 0, buffer, sizeof(buffer));
			sip_uac_add_header(t, "Authorization", buffer);
			assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
			break;

		case HTTP_AUTHENTICATION_BASIC:
			assert(0);
			break;

		default:
			assert(0);
		}
	}
	else
	{
	}
	return 0;
}

static void sip_uac_register_test(struct sip_uac_test2_t *test)
{
	struct sip_uac_transaction_t* t;
	//t = sip_uac_register(uac, "Bob <sip:bob@biloxi.com>", "sip:registrar.biloxi.com", 7200, sip_uac_message_onregister, test);
	t = sip_uac_register(test->sip, SIP_FROM, "sip:" SIP_HOST, SIP_EXPIRED, sip_uac_onregister, test);
	assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
}

static int STDCALL TimerThread(void* param)
{
	bool *running = (bool*)param;
	while (*running)
	{
		aio_timeout_process();
		system_sleep(5);
	}
	return 0;
}

static int sip_uac_test_process(struct sip_uac_test2_t* test)
{
    uint8_t buffer[4 * 1024];
    http_parser_t* parser;
	http_parser_t* request;
	http_parser_t* response;

	request = http_parser_create(HTTP_PARSER_CLIENT);
	response = http_parser_create(HTTP_PARSER_SERVER);

    do
    {
		memset(buffer, 0, sizeof(buffer));
        test->addrlen = sizeof(test->addr);
		int r = socket_recvfrom(test->udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&test->addr, &test->addrlen);
		if (-1 == r && EINTR == errno)
			continue;

		printf("\n%s\n", buffer);
		parser = 0 == strncasecmp("SIP", (char*)buffer, 3) ? request : response;

		size_t n = r;
		assert(0 == http_parser_input(parser, buffer, &n));
		struct sip_message_t* msg = sip_message_create(parser == response ? SIP_MESSAGE_REQUEST : SIP_MESSAGE_REPLY);
		assert(0 == sip_message_load(msg, parser));
		assert(0 == sip_agent_input(test->sip, msg));
		sip_message_destroy(msg);
		http_parser_clear(parser);
    } while (1);
    
	http_parser_destroy(request);
	http_parser_destroy(response);
    return 0;
}

void sip_uac_test2(void)
{
	socket_init();
	struct sip_uac_test2_t test;
	test.running = true;
	test.transport = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};
    struct sip_uas_handler_t handler = {
		sip_uas_oninvite,
		sip_uas_onack,
		sip_uas_onbye,
		sip_uas_oncancel,
		sip_uas_onregister,
		sip_uas_onrequest,
		sip_uas_transport_send,
	};

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, STUN_SERVER, STUN_PORT));

	pthread_t th;
	thread_create(&th, TimerThread, &test.running);

	test.udp = socket_udp();
	test.sip = sip_agent_create(&handler, &test);
    socket_bind_any(test.udp, SIP_PORT);
	sip_uac_register_test(&test);
    
    sip_uac_test_process(&test);

	thread_destroy(th);
	sip_agent_destroy(test.sip);
	socket_close(test.udp);
	socket_cleanup();
}
