#include "sockutil.h"
#include "aio-timeout.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "rtsp-media.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "mov-format.h"
#include "rtp.h"
#include "../test/rtp-socket.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "md5.h"
#include "cpm/shared_ptr.h"
#include "../test/media/mp4-file-reader.h"
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#define SIP_USR "sipuser1"
#define SIP_PWD "1234567890"
#define SIP_HOST "sip.linphone.org"
#define SIP_FROM "sip:sipuser1@sip.linphone.org"
#define SIP_PEER "sip:sipuser2@sip.linphone.org"
#define HOST "192.168.3.34"

extern "C" uint32_t rtp_ssrc(void);
extern "C" void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding);
extern "C" int sdp_h264(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_h265(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_opus(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_aac_latm(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_aac_generic(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_g711u(uint8_t *data, int bytes, unsigned short port);
extern "C" int sdp_g711a(uint8_t *data, int bytes, unsigned short port);

struct sip_uac_test2_session_t
{
	struct rtsp_media_t medias[3];
	int nmedia;

	struct rtp_media_t
	{
		struct rtsp_media_t* m;
		int fmt;

		enum AVPACKET_CODEC_ID codec;
		void* decoder;
		void* encoder;
		void* rtp;

		uint16_t seq;
		uint32_t ssrc;
		uint32_t timestamp;
		uint32_t frequency;
		uint32_t bandwidth;

		socket_t udp[2];
		unsigned short port[2];
	} audio, video;

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

static int sdp_media_audio_negotiate(const struct rtsp_media_t* m)
{
	int i;
	assert(0 == strcasecmp("IP4", m->addrtype) || 0 == strcasecmp("IP6", m->addrtype));
	for (i = 0; i < m->avformat_count; i++)
	{
		if (m->avformats[i].fmt == RTP_PAYLOAD_PCMA)
			return i;
	}
	return -1;
}

static int sdp_media_video_negotiate(const struct rtsp_media_t* m)
{
	int i;
	assert(0 == strcasecmp("IP4", m->addrtype) || 0 == strcasecmp("IP6", m->addrtype));
	for (i = 0; i < m->avformat_count; i++)
	{
		if (m->avformats[i].fmt >= 96 && 0 == strcmp(m->avformats[i].encoding, "H264"))
			return i;
	}
	return -1;
}

static void rtp_onaudio_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
}

static void rtp_onvideo_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
}

static void* rtp_audio_alloc(void* param, int bytes)
{
	struct media_t* m = (struct media_t*)param;
	assert(bytes <= sizeof(m->packet));
	return m->packet;
}

static void rtp_audio_free(void* param, void *packet)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);
}

static void rtp_audio_packet(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);

	int r = m->transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
	rtp_onsend(m->rtp, packet, bytes/*, time*/);
}

static void* rtp_video_alloc(void* param, int bytes)
{
	struct media_t* m = (struct media_t*)param;
	assert(bytes <= sizeof(m->packet));
	return m->packet;
}

static void rtp_video_free(void* param, void *packet)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);
}

static void rtp_video_packet(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);

	int r = m->transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
	rtp_onsend(m->rtp, packet, bytes/*, time*/);
}

static void rtp_audio_onrtcp(void* param, const struct rtcp_msg_t* msg)
{
}

static void rtp_video_onrtcp(void* param, const struct rtcp_msg_t* msg)
{
}

static void mp4_onvideo(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	int n = 0;
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;

	s->video.seq = (uint16_t)rtp_ssrc();
	s->video.ssrc = rtp_ssrc();
	s->video.timestamp = rtp_ssrc();
	s->video.bandwidth = 1000000;
	rtp_socket_create(HOST, s->video.udp, s->video.port);

	if (MOV_OBJECT_H264 == object)
	{
		s->video.frequency = 90000;
		s->video.codec = AVCODEC_VIDEO_H264;
		sdp_h264((uint8_t*)videofmt, sizeof(videofmt) - 1, s->video.port[0], RTP_PAYLOAD_H264, s->video.frequency, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		s->video.frequency = 90000;
		s->video.codec = AVCODEC_VIDEO_H265;
		sdp_h264((uint8_t*)videofmt, sizeof(videofmt) - 1, s->video.port[0], RTP_PAYLOAD_H265, s->video.frequency, extra, bytes);
	}
	else if (MOV_OBJECT_MP4V == object)
	{
		s->video.frequency = 90000;
		s->video.codec = AVCODEC_VIDEO_MPEG4;
		sdp_h264((uint8_t*)videofmt, sizeof(videofmt) - 1, s->video.port[0], RTP_PAYLOAD_MP4V, s->video.frequency, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		rtp_video_alloc,
		rtp_video_free,
		rtp_video_packet,
	};
	s->video.encoder = rtp_payload_encode_create(RTP_PAYLOAD_H264, "H264", s->video.seq, s->video.ssrc, &rtpfunc, s);

	struct rtp_event_t event;
	event.on_rtcp = rtp_video_onrtcp;
	s->video.rtp = rtp_create(&event, s, s->video.ssrc, s->video.timestamp, s->video.frequency, s->video.bandwidth, 1);
}

static void mp4_onaudio(void* param, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	int n = 0;
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	m->pkts = avpacket_queue_create(100);
	m->track = track;
	m->rtcp_clock = 0;
	m->ssrc = rtp_ssrc();
	m->timestamp = rtp_ssrc();
	m->bandwidth = 128 * 1024;
	m->dts_first = -1;
	m->dts_last = -1;

	if (MOV_OBJECT_AAC == object || MOV_OBJECT_AAC_LOW == object)
	{
		mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &self->m_aac);

		if (1)
		{
			// RFC 6416
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MP4A-LATM");
			n = sdp_aac_latm(self->m_packet, sizeof(self->m_packet), 0, RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
		else
		{
			// RFC 3640 3.3.1. General (p21)
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MPEG4-GENERIC");
			n = sdp_aac_generic(self->m_packet, sizeof(self->m_packet), 0, RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
	}
	else if (MOV_OBJECT_OPUS == object)
	{
		// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_OPUS;
		snprintf(m->name, sizeof(m->name), "%s", "opus");
		n = sdp_opus(self->m_packet, sizeof(self->m_packet), 0, RTP_PAYLOAD_OPUS, sample_rate, channel_count, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_PCMU;
		snprintf(m->name, sizeof(m->name), "%s", "PCMU");
		n = sdp_g711u(self->m_packet, sizeof(self->m_packet), 0);
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		MP4FileSource::RTPAlloc,
		MP4FileSource::RTPFree,
		MP4FileSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)rtp_ssrc(), m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, self, m->ssrc, m->timestamp, m->frequency, m->bandwidth, 1);

	n += snprintf((char*)self->m_packet + n, sizeof(self->m_packet) - n, "a=control:track%d\n", m->track);
	self->m_sdp += (const char*)self->m_packet;
}

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	const char* pattern = "v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Talk\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

	char reply[1024];
	char audiofmt[64];
	char videofmt[256];
	const cstring_t* h = sip_message_get_header_by_name(req, "Content-Type");
	if (0 == cstrcasecmp(h, "application/sdp"))
	{
		socklen_t len = 0;
		std::shared_ptr<sip_uac_test2_session_t> s(new sip_uac_test2_session_t());
		s->nmedia = rtsp_media_sdp((const char*)data, s->medias, sizeof(s->medias) / sizeof(s->medias[0]));
		for (int i = 0; i < s->nmedia; i++)
		{
			struct rtsp_media_t* m = s->medias + i;
			if (0 == strcmp("audio", m->media))
			{
				int j = sdp_media_audio_negotiate(m);
				if(-1 == j)
					continue;

				struct rtp_payload_t handler;
				handler.alloc = NULL;
				handler.free = NULL;
				handler.packet = rtp_onaudio_packet;
				s->audio.decoder = rtp_payload_decode_create(m->avformats[j].fmt, m->avformats[j].encoding, &handler, s.get());
				if (NULL == s->audio.decoder)
					return; // ignore

				s->audio.m = m;
				s->audio.fmt = j;
				s->audio.seq = (uint16_t)rtp_ssrc();
				s->audio.ssrc = rtp_ssrc();
				s->audio.timestamp = rtp_ssrc();
				s->audio.frequency = 90000;
				s->audio.bandwidth = 1000000;
				rtp_socket_create(HOST, s->audio.udp, s->audio.port);

				struct rtp_payload_t rtpfunc = {
					rtp_audio_alloc,
					rtp_audio_free,
					rtp_audio_packet,
				};
				s->audio.encoder = rtp_payload_encode_create(RTP_PAYLOAD_PCMA, "H264", s->audio.seq, s->audio.ssrc, &rtpfunc, s.get());

				struct rtp_event_t event;
				event.on_rtcp = rtp_audio_onrtcp;
				s->audio.rtp = rtp_create(&event, s.get(), s->audio.ssrc, s->audio.timestamp, s->audio.frequency, s->audio.bandwidth, 1);

				sdp_g711a((uint8_t*)audiofmt, sizeof(audiofmt) - 1, s->audio.port[0]);
			}
			else if (0 == strcmp("video", m->media))
			{
				int j = sdp_media_video_negotiate(m);
				if (-1 == j)
					continue;
				
				s->video.m = m;
				s->video.fmt = j;


				struct rtp_payload_t handler;
				handler.alloc = NULL;
				handler.free = NULL;
				handler.packet = rtp_onvideo_packet;
				s->video.decoder = rtp_payload_decode_create(s->video.m->avformats[s->video.fmt].fmt, s->video.m->avformats[s->video.fmt].encoding, &handler, s);
				if (NULL == s->video.decoder)
					return; // ignore

			}
		}

		std::shared_ptr<AVPacketQueue> pkts(new AVPacketQueue(200));
		std::shared_ptr<MP4FileReader> reader(new MP4FileReader("1.mp4"));
		struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio};
		reader->GetInfo(&info, s.get());
		std::shared_ptr<VodFileSource> source(new VodFileSource(reader, pkts));
		s->source = source;

		sip_uas_add_header(t, "Content-Type", "application/sdp");
		sip_uas_add_header(t, "Contact", "sip:" SIP_USR "@" HOST);
		snprintf(reply, sizeof(reply), pattern, SIP_USR, HOST, HOST, audiofmt, videofmt);
		assert(0 == sip_uas_reply(t, 200, reply, strlen(reply)));
		return s.get();
	}
	else
	{
		assert(0);
		return NULL;
	}
}

static int STDCALL rtsp_play_thread(void* param)
{
	struct sip_media_t* m = (struct sip_media_t*)param;
	m->source->SetTransport("track1", m->transport);
	while (1)
	{
		m->source->Play();
		system_sleep(10);
	}

	delete m;
	return 0;
}

/// @param[in] code 0-ok, other-sip status code
/// @return 0-ok, other-error
static void sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	struct sip_uac_test2_session_t* m = (struct sip_uac_test2_session_t*)session;

	if (200 <= code && code < 300)
	{
		pthread_t th;
		thread_create(&th, rtsp_play_thread, m);
	}
01	else
	{
		//delete m;
		assert(0);
	}
}

/// on terminating a session(dialog)
static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
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

static void sip_uac_invite_test(struct sip_uac_test2_t *test)
{
	const char* pattern = "v=0\n"
		"o=- 0 0 IN IP4 %s\n"
		"s=Play\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"m=audio %hu RTP/AVP 8\n"
		//"a=rtpmap:96 PS/90000\n"
		//"a=fmtp:98 profile-level-id=42800D;packetization-mode=1;sprop-parameter-sets=Z0KADYiLULBLQgAAIygAAr8gCAAAAAAB,aM44gAAAAAE=\n"
		;

    char buffer[1024];
    struct sip_uac_transaction_t* t;
    t = sip_uac_invite(test->sip, SIP_FROM, SIP_PEER, sip_uac_oninvited, test);
    ++test->auth.nc;
    snprintf(test->auth.uri, sizeof(test->auth.uri), "%s", SIP_PEER);
    snprintf(test->auth.username, sizeof(test->auth.username), "%s", SIP_USR);
    http_header_auth(&test->auth, SIP_PWD, "INVITE", NULL, 0, buffer, sizeof(buffer));
    sip_uac_add_header(t, "Proxy-Authorization", buffer);
	sip_uac_add_header(t, "Content-Type", "application/sdp");

	//socklen_t len = 0;
	//struct sip_media_t* m = new sip_media_t;
	//m->transport.reset(new RTPUdpTransport());
	//m->nmedia = rtsp_media_sdp((const char*)data, m->medias, sizeof(m->medias) / sizeof(m->medias[0]));
	//assert(m->nmedia > 0);
	//assert(0 == strcasecmp("IP4", m->medias[0].addrtype) || 0 == strcasecmp("IP6", m->medias[0].addrtype));
	//m->port[0] = m->medias[0].port[0];
	//m->port[1] = m->medias[0].nport > 1 ? m->medias[0].port[1] : (m->medias[0].port[0] + 1);
	//assert(0 == ((RTPUdpTransport*)m->transport.get())->Init(m->medias[0].address, m->port));
	//m->source.reset(new PCMFileSource("C:\\Users\\Administrator\\sintel-1280.pcm"));
	//std::string sdp;
	//m->source->GetSDPMedia(sdp);

	//snprintf(buffer, sizeof(buffer), pattern, HOST, HOST, m->port[0]);
    assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
}

static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	char buffer[1024];
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	if (200 <= code && code < 300)
	{
		printf("Register OK\n");
		//sip_uac_invite_test(test);
	}
	else if (401 == code)
	{
		// https://blog.csdn.net/yunlianglinfeng/article/details/81109380
		// http://www.voidcn.com/article/p-oqqbqgvd-bgn.html
		const cstring_t* h;
		t = sip_uac_register(test->sip, SIP_FROM, SIP_PEER, 600, sip_uac_onregister, param);
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
	t = sip_uac_register(test->sip, SIP_FROM, "sip:" SIP_HOST, 600, sip_uac_onregister, test);
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
    uint8_t buffer[2 * 1024];
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
