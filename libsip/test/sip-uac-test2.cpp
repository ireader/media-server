#include <map>
#include <memory>
#include <string>
#include "sockutil.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/pollfd.h"
#include "port/network.h"
#include "port/ip-route.h"
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
#include "../test/ice-transport.h"
#include "../test/media/mp4-file-reader.h"

#define SIP_PWD "1234"
#define SIP_HOST "119.23.15.234"
#define SIP_FROM "sip:1002@119.23.15.234"
#define SIP_PEER "sip:1001@192.168.1.100"
#define SIP_EXPIRED 60
#define TURN_SERVER NULL
#define TURN_USR "test"
#define TURN_PWD "123456"

extern "C" void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding);
static int rtp_packet_send(void* param, const void *packet, int bytes);

struct sip_uac_transport_address_t
{
	socket_t udp;
	socket_t tcp;
	socklen_t addrlen;
	struct sockaddr_storage addr;
};

struct sip_uac_test2_session_t
{
    char buffer[2 * 1024 * 1024];
	std::string user;
	std::string from;
	union
	{
		struct sip_uas_transaction_t* t;
		struct sip_uac_transaction_t* tuac;
	};
	struct ice_transport_t* avt;
	struct sip_uac_transport_address_t transport;

	struct rtsp_media_t medias[3]; // peer sdp media
	int nmedia;

	struct av_media_t
	{
		int stream; // stream index, base 0
		int connected;
		struct sip_uac_test2_session_t* s;

		char sdp[1204];
		struct rtsp_media_t* m;
		int fmt;

		enum AVPACKET_CODEC_ID codec;
		void* decoder;

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

	bool running;
	pthread_t th;
	std::shared_ptr<AVPacketQueue> pkts;
	std::shared_ptr<VodFileSource> source;
};

struct sip_uac_test2_t
{
	bool running;
	char usr[64];
	char local[65];

	socket_t udp;
	socket_t tcp;
	struct sip_agent_t* sip;
	struct sip_uac_transport_address_t transport; // for register
	struct http_header_www_authenticate_t auth;
	int nonce_count;
	char callid[64];
	int cseq;
    
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

	struct sip_uac_transport_address_t *t = (struct sip_uac_transport_address_t *)transport;

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

	t->addrlen = sizeof(t->addr);
	memset(&t->addr, 0, sizeof(t->addr));
	strcpy(protocol, socket_invalid == t->tcp ? "UDP" : "TCP");

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
	r = socket_addr_from(&t->addr, &t->addrlen, uri->host, uri->port ? uri->port : SIP_PORT);
	if (0 == r)
	{
		socket_addr_to((struct sockaddr*)&t->addr, t->addrlen, ip, &port);
		socket_getname(socket_invalid == t->tcp ? t->udp : t->tcp, local, &port);
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
	struct sip_uac_transport_address_t *t = (struct sip_uac_transport_address_t *)transport;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	//printf("%s\n", (const char*)data);
	int r = socket_sendto(socket_invalid == t->tcp ? t->udp : t->tcp, data, bytes, 0, (struct sockaddr*)&t->addr, t->addrlen);
	return r == bytes ? 0 : -1;
}

static int sip_uas_transport_send(void* param, const struct cstring_t* peer, const void* data, int bytes)
{
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	std::shared_ptr<uri_t> uri(uri_parse(peer->p, peer->n), uri_free);
	if (!uri.get())
		return -1; // invalid uri

	socklen_t addrlen;
	struct sockaddr_storage addr;
	int r = socket_addr_from(&addr, &addrlen, uri->host, uri->port ? uri->port : SIP_PORT);
	if (0 != r)
		return -1; // invalid

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	//printf("%s\n\n", (const char*)data);
	r = socket_sendto(socket_invalid == test->tcp ? test->udp : test->tcp, data, bytes, 0, (struct sockaddr*)&addr, addrlen);
	return r == bytes ? 0 : -1;
}

static int sdp_media_negotiate(const struct rtsp_media_t* m)
{
	int i, j;
    const int payloads[] = { RTP_PAYLOAD_PCMA, RTP_PAYLOAD_PCMU };
    const char* payloads2[] = { "H264", "H265", "MP4V-ES", "MP4A-LATM", "MPEG4-GENERIC", "opus" };
    
	assert(0 == strcasecmp("IP4", m->addrtype) || 0 == strcasecmp("IP6", m->addrtype));
	for (i = 0; i < m->avformat_count; i++)
	{
		if(m->avformats[i].fmt < 96)
        {
            //for(j = 0; j < sizeof(payloads)/sizeof(payloads[0]); j++)
            //{
            //    if(payloads[j] == m->avformats[i].fmt)
            //        return i;
            //}
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

//static uint8_t sdp_media_find(const struct rtsp_media_t* medias, int count, const char* media, uint8_t payload, const char* encoding)
//{
//	int i, j;
//	const struct rtsp_media_t* m;
//	for (i = 0; i < count; i++)
//	{
//		m = medias + i;
//		if(0 != strcmp(m->media, media))
//			continue;
//
//		if (payload < 96)
//		{
//			for (j = 0; j < m->avformat_count; j++)
//			{
//				if (payload == m->avformats[j].fmt)
//					return payload;
//			}
//		}
//		else
//		{
//			for (j = 0; j < m->avformat_count; j++)
//			{
//				if (0 == strcmp(m->avformats[j].encoding, encoding))
//					return m->avformats[j].fmt;
//			}
//		}
//	}
//	return payload; // invalid
//}

static void mp4_onvideo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
	s->video.track = track;
	s->video.fp = fopen("sipvideo.h264", "wb");
	s->video.s = s;
	s->video.sender.send = rtp_packet_send;
	s->video.sender.param = &s->video;

	char ip[SOCKET_ADDRLEN];
	u_short port;
	struct sockaddr_storage addr[2];
	assert(0 == ice_transport_getaddr(s->avt, s->video.stream, 1, &addr[0]));
	assert(0 == ice_transport_getaddr(s->avt, s->video.stream, 2, &addr[1]));
	socket_addr_to((struct sockaddr*)&addr[0], socket_addr_len((struct sockaddr*)&addr[0]), ip, &port);

	if (MOV_OBJECT_H264 == object)
	{
		s->video.codec = AVCODEC_VIDEO_H264;
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s->video.u.avc);
		rtp_sender_init_video(&s->video.sender, port, RTP_PAYLOAD_H264, "H264", width, height, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		s->video.codec = AVCODEC_VIDEO_H265;
        mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s->video.u.hevc);
        rtp_sender_init_video(&s->video.sender, port, RTP_PAYLOAD_H265, "H265", width, height, extra, bytes);
	}
	else if (MOV_OBJECT_MP4V == object)
	{
		s->video.codec = AVCODEC_VIDEO_MPEG4;
        rtp_sender_init_video(&s->video.sender, port, RTP_PAYLOAD_MP4V, "MP4V-ES", width, height, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	int n = snprintf(s->video.sdp, sizeof(s->video.sdp), "%s", (char*)s->video.sender.buffer);
	n += ice_transport_getsdp(s->avt, s->video.stream, (char*)s->video.sender.buffer + n, sizeof(s->video.sender.buffer) - n);
}

static void mp4_onaudio(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)param;
	s->audio.track = track;
	s->audio.fp = fopen("sipaudio.pcm", "wb");
	s->audio.s = s;
	s->audio.sender.send = rtp_packet_send;
	s->audio.sender.param = &s->audio;

	char ip[SOCKET_ADDRLEN];
	u_short port;
	struct sockaddr_storage addr[2];
	assert(0 == ice_transport_getaddr(s->avt, s->audio.stream, 1, &addr[0]));
	assert(0 == ice_transport_getaddr(s->avt, s->audio.stream, 2, &addr[1]));
	socket_addr_to((struct sockaddr*)&addr[0], socket_addr_len((struct sockaddr*)&addr[0]), ip, &port);

	if (MOV_OBJECT_AAC == object || MOV_OBJECT_AAC_LOW == object)
	{
        s->audio.codec = AVCODEC_AUDIO_AAC;
        mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s->audio.u.aac);
        rtp_sender_init_audio(&s->audio.sender, port, RTP_PAYLOAD_MP4A, "MP4A-LATM", channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
	else if (MOV_OBJECT_OPUS == object)
	{
        s->audio.codec = AVCODEC_AUDIO_OPUS;
        rtp_sender_init_audio(&s->audio.sender, port, RTP_PAYLOAD_OPUS, "opus",channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
        s->audio.codec = AVCODEC_AUDIO_PCM;
        rtp_sender_init_audio(&s->audio.sender, port, RTP_PAYLOAD_PCMU, "", channel_count, bit_per_sample, sample_rate, extra, bytes);
	}
    else if (MOV_OBJECT_G711a == object)
    {
        s->audio.codec = AVCODEC_AUDIO_PCM;
        rtp_sender_init_audio(&s->audio.sender, port, RTP_PAYLOAD_PCMA, "", channel_count, bit_per_sample, sample_rate, extra, bytes);
    }
    else
	{
		assert(0);
		return;
	}

	int n = snprintf(s->audio.sdp, sizeof(s->audio.sdp), "%s", (char*)s->audio.sender.buffer);
	n += ice_transport_getsdp(s->avt, s->audio.stream, (char*)s->audio.sender.buffer + n, sizeof(s->audio.sender.buffer) - n);
}

static void ice_transport_onconnected(void* param, int64_t streams)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	for (int stream = 0; stream < 2; stream++)
	{
		sip_uac_test2_session_t::av_media_t* av = s->video.stream == stream ? &s->video : &s->audio;
		av->connected = (streams & ((int64_t)1 << stream)) ? 1 : 0;
		//for (int component = 0; component < 2; component++)
		//{
		//	assert(0 == ice_transport_get_candidate(s->ice, av->stream, component + 1, &av->local[component]));
		//}
	}

	printf("ice_transport_onconnected 0x%x\n", (unsigned int)streams);
	s->source->Play();

	// TODO: reinvite
}

static void ice_transport_onbind(void* param, int code)
{
	const char* pattern = "v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Talk\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

	char reply[4*1024];
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	if (0 == code)
	{
		std::shared_ptr<MP4FileReader> reader(new MP4FileReader("e:\\video\\mp4\\name.opus.mp4"));
		struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
		reader->GetInfo(&info, s);
		std::shared_ptr<VodFileSource> source(new VodFileSource(reader, s->pkts));
		s->source = source;

		// default connect address
		u_short port;
		char host[SOCKET_ADDRLEN];
		struct sockaddr_storage addr;
		ice_transport_getaddr(s->avt, 0, 1, &addr);
		socket_addr_to((struct sockaddr*)&addr, socket_addr_len((struct sockaddr*)&addr), host, &port);

		// TODO: PRACK add 100rel/precondiation
		//sip_uac_add_header(s->tuac, "Supported", "100ref");
		//sip_uac_add_header(s->tuac, "Supported", "precondition");
		// TODO: add Recv-Info
		//sip_uac_add_header(s->tuac, "Recv-Info", "");
		// TODO: add Allow-Events
		//sip_uac_add_header(s->tuac, "Allow-Events", "");

		sip_uas_add_header(s->t, "Content-Type", "application/sdp");
		//sip_uas_add_header(s->t, "Contact", "sip:" SIP_USR "@" LOCAL_HOST);
		snprintf(reply, sizeof(reply), pattern, s->user.c_str(), host, host, s->audio.m ? (char*)s->audio.sender.buffer : "", s->video.m ? (char*)s->video.sender.buffer : "");
		assert(0 == sip_uas_reply(s->t, 200, reply, strlen(reply)));

		ice_transport_connect(s->avt, s->medias, s->nmedia);
	}
	else
	{
		assert(0 == sip_uas_reply(s->t, 501, "Internal Server Error", 21));
	}
}

static void ice_transport_ondata(void* param, int stream, int component, const void* data, int bytes)
{
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;
	sip_uac_test2_session_t::av_media_t* av = s->audio.stream == stream ? &s->audio : &s->video;
	if (1 == component)
	{
		assert(rtp_payload_decode_input(av->decoder, data, bytes) >= 0);
		//assert(0 == rtp_onreceived(av->sender.rtp, data, bytes));
	}
	else
	{
		//assert(0 == rtp_onreceived_rtcp(av->decoder, data, bytes));
	}
}

static void rtp_packet_onrecv(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
	sip_uac_test2_session_t::av_media_t* av = (sip_uac_test2_session_t::av_media_t*)param;
	fwrite(packet, 1, bytes, av->fp);
}

static int rtp_packet_send(void* param, const void *packet, int bytes)
{
	sip_uac_test2_session_t::av_media_t* av = (sip_uac_test2_session_t::av_media_t*)param;
	return ice_transport_send(av->s->avt, av->stream, 1, packet, bytes);
}

static int rtcp_report(struct sip_uac_test2_session_t::av_media_t* av, time64_t clock)
{
	int r;
	int interval = rtp_rtcp_interval(av->sender.rtp);
	if (av->clock + interval < clock)
	{
		r = rtp_rtcp_report(av->sender.rtp, av->sender.buffer, sizeof(av->sender.buffer));
		r = ice_transport_send(av->s->avt, av->stream, 2, av->sender.buffer, r);
		//printf("rtcp_report: %d\n", r);
		av->clock = clock;
	}
	return 0;
}

static int STDCALL sip_work_thread(void* param)
{
	time64_t clock;

	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	while (s->running)
	{
		clock = time64_now();

		// RTCP report
		if (s->audio.sender.rtp && s->audio.connected)
			rtcp_report(&s->audio, clock);
		if (s->video.sender.rtp && s->video.connected)
			rtcp_report(&s->video, clock);

		for (std::shared_ptr<avpacket_t> pkt(s->pkts->FrontWait(0), avpacket_release); pkt.get(); pkt.reset(s->pkts->FrontWait(0), avpacket_release))
		{
			if (pkt->stream == s->audio.track && s->audio.m)
			{
				uint32_t timestamp = s->audio.sender.timestamp + (uint32_t)(pkt->pts * (s->audio.sender.frequency / 1000) /*kHz*/);
				rtp_payload_encode_input(s->audio.sender.encoder, pkt->data, pkt->size, timestamp);
				//printf("send audio[%d] packet pts: %" PRId64 ", timestamp: %u\n", s->audio.sender.frequency, pkt->pts, timestamp);
			}
			else if (pkt->stream == s->video.track && s->video.m)
			{
				int n = h264_mp4toannexb(&s->video.u.avc, pkt->data, pkt->size, s->buffer, sizeof(s->buffer));
				uint32_t timestamp = s->video.sender.timestamp + (uint32_t)(pkt->pts * (s->video.sender.frequency / 1000) /*kHz*/);
				rtp_payload_encode_input(s->video.sender.encoder, s->buffer, n, timestamp);
				//printf("send video[%d] packet pts: %" PRId64 ", timestamp: %u\n", s->video.sender.frequency, pkt->pts, timestamp);
			}
			else
			{
				//assert(0);
				// ignore
			}

			s->pkts->Pop();
		}
	}

	//size_t n = rtp_rtcp_bye(m->rtp, rtcp, sizeof(rtcp));
	s->source->Pause();
	return 0;
}

static void* sip_uas_oninvite_indialog(sip_uac_test2_session_t* s, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	const char* pattern = "v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Talk\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

	// default connect address
	u_short port;
	char host[SOCKET_ADDRLEN];
	struct sockaddr_storage addr;
	ice_transport_getaddr(s->avt, 0, 1, &addr);
	socket_addr_to((struct sockaddr*)&addr, socket_addr_len((struct sockaddr*)&addr), host, &port);

	char reply[4 * 1024];
	int n = snprintf(reply, sizeof(reply), pattern, s->user.c_str(), host, host, "", "");
	if (s->audio.m)
	{
		n += snprintf(reply + n, sizeof(reply) - n, "%s", s->audio.sdp);
		n += ice_transport_getsdp(s->avt, s->audio.stream, reply + n, sizeof(reply) - n);
	}
	if (s->video.m)
	{
		n += snprintf(reply + n, sizeof(reply) - n, "%s", s->video.sdp);
		n += ice_transport_getsdp(s->avt, s->video.stream, reply + n, sizeof(reply) - n);
	}

	s->t = t;
	sip_uas_add_header(t, "Content-Type", "application/sdp");
	//sip_uas_add_header(s->t, "Contact", "sip:" SIP_USR "@" LOCAL_HOST);
	assert(0 == sip_uas_reply(t, 200, reply, strlen(reply)));
	return s;
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

	std::shared_ptr<sip_uac_test2_session_t> s(new sip_uac_test2_session_t());
	s->from = std::string(req->from.uri.host.p, req->from.uri.host.n);
	{
		sip_uac_test2_t::TSessions::iterator it;
		AutoThreadLocker locker(ctx->locker);
		it = ctx->sessions.find(s->from);
		if (ctx->sessions.end() != it)
		{
			if(!dialog)
				return NULL; // ignore

			// in dialog
			return sip_uas_oninvite_indialog(it->second.get(), req, t, dialog, data, bytes);
		}
		ctx->sessions.insert(std::make_pair(s->from, s));
	}

	struct ice_transport_handler_t handler = {
		ice_transport_ondata,
		ice_transport_onbind,
		ice_transport_onconnected,
	};

	s->pkts = std::shared_ptr<AVPacketQueue>(new AVPacketQueue(200));
	s->avt = ice_transport_create(0, &handler, s.get());
	memset(&s->audio, 0, sizeof(s->audio));
	memset(&s->video, 0, sizeof(s->video));
	s->user = ctx->usr;
	s->t = t;

	s->nmedia = rtsp_media_sdp((const char*)data, s->medias, sizeof(s->medias) / sizeof(s->medias[0]));
	for (int i = 0; i < s->nmedia; i++)
	{
		struct rtsp_media_t* m = s->medias + i;

		if (0 == strcmp("audio", m->media))
		{
			int j = sdp_media_negotiate(m);
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
            //socket_addr_from(&s->audio.remote[0], &addrlen, m->address, m->port[0]);
			//socket_addr_from(&s->audio.remote[1], &addrlen, m->address, m->port[1]);
		}
		else if (0 == strcmp("video", m->media))
		{
			int j = sdp_media_negotiate(m);
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
			//socket_addr_from(&s->video.remote[0], &addrlen, m->address, m->port[0]);
			//socket_addr_from(&s->video.remote[1], &addrlen, m->address, m->port[1]);
		}
	}

	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	socket_addr_from_ipv4((struct sockaddr_in*)&stun, TURN_SERVER, STUN_PORT);
	ice_transport_bind(s->avt, s->nmedia, 2, TURN_SERVER ? (struct sockaddr*)&stun : NULL, 0, TURN_USR, TURN_PWD);

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
static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	struct sip_uac_test2_t* ctx = (struct sip_uac_test2_t*)param; assert(ctx);
	sip_uac_test2_session_t* s = (sip_uac_test2_session_t*)session; assert(s);
	printf("sip_uas_onack[%p]: %d\n", s, code);

	if (200 <= code && code < 300)
	{
		//if(s->source.get())
		//	s->source->Play();
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
	return 0;
}

/// on terminating a session(dialog)
static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_uac_test2_t* ctx = (struct sip_uac_test2_t*)param;
	sip_uac_test2_session_t* p = (sip_uac_test2_session_t*)session;

	std::shared_ptr<sip_uac_test2_session_t> s;
	sip_uac_test2_t::TSessions::iterator it;
	{
		AutoThreadLocker locker(ctx->locker);
		it = ctx->sessions.find(p->from);
		if (it == ctx->sessions.end())
			return sip_uas_reply(t, 481, NULL, 0);

		s = it->second;
		ctx->sessions.erase(it);
	}

	s->running = false;
	s->source->Pause();
	thread_destroy(s->th);

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

static int sip_uas_onmessage(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

static void sip_uac_ice_transport_onbind(void* param, int code)
{
	const char* pattern = "v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Talk\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"%s%s"; // audio/video

	char buffer[4*1024];
	struct sip_uac_test2_session_t* s = (struct sip_uac_test2_session_t*)param;

	if (0 == code)
	{
		std::shared_ptr<MP4FileReader> reader(new MP4FileReader("e:\\video\\mp4\\name.opus.mp4"));
		struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
		reader->GetInfo(&info, s);
		std::shared_ptr<VodFileSource> source(new VodFileSource(reader, s->pkts));
		s->source = source;

		// default connect address
		u_short port;
		char host[SOCKET_ADDRLEN];
		struct sockaddr_storage addr;
		ice_transport_getaddr(s->avt, 0, 1, &addr);
		socket_addr_to((struct sockaddr*)&addr, socket_addr_len((struct sockaddr*)&addr), host, &port);

		// TODO: PRACK add 100rel/precondiation
		//sip_uac_add_header(s->tuac, "Supported", "100ref");
		//sip_uac_add_header(s->tuac, "Supported", "precondition");
		// TODO: add Recv-Info
		//sip_uac_add_header(s->tuac, "Recv-Info", "");
		// TODO: add Allow-Events
		//sip_uac_add_header(s->tuac, "Allow-Events", "");

		sip_uac_add_header(s->tuac, "Content-Type", "application/sdp");
		int n = snprintf(buffer, sizeof(buffer), pattern, s->user.c_str(), host, host, (char*)s->audio.sender.buffer, (char*)s->video.sender.buffer);

		struct sip_transport_t t = {
			sip_uac_transport_via,
			sip_uac_transport_send,
		};
		assert(0 == sip_uac_send(s->tuac, buffer, n, &t, &s->transport));
	}
	else
	{
		assert(0);
	}
}

static void* sip_uac_oninvited(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
	const cstring_t* h;
	std::shared_ptr<struct sip_uac_test2_session_t> s;
	struct sip_uac_test2_t *ctx = (struct sip_uac_test2_t*)param;

	if (reply)
	{
		sip_uac_test2_t::TSessions::iterator it;
		AutoThreadLocker locker(ctx->locker);
		it = ctx->sessions.find(SIP_PEER);
		if (it == ctx->sessions.end())
			return NULL; // ignore
		s = it->second;
	}

	// TODO: handle reply->recv_info

	if (200 == code)
	{
		h = sip_message_get_header_by_name(reply, "Content-Type");
		if (!h || 0 != cstrcasecmp(h, "application/sdp"))
		{
			assert(0);
			return NULL;
		}

		s->nmedia = rtsp_media_sdp((const char*)reply->payload, s->medias, sizeof(s->medias) / sizeof(s->medias[0]));
		assert(0 == ice_transport_connect(s->avt, s->medias, s->nmedia));
		for (int i = 0; i < s->nmedia; i++)
		{
			struct rtsp_media_t* m = s->medias + i;

			if (0 == strcmp("audio", m->media))
			{
				int j = sdp_media_negotiate(m);
				if (-1 == j)
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

				assert(i == s->audio.stream);
				s->audio.m = m;
				s->audio.fmt = j;
			}
			else if (0 == strcmp("video", m->media))
			{
				int j = sdp_media_negotiate(m);
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

				assert(i == s->video.stream);
				s->video.m = m;
				s->video.fmt = j;
			}
		}

		s->from = SIP_PEER;
		s->running = true;
		thread_create(&s->th, sip_work_thread, s.get());
		return s.get();
	}
	else if (code == 183)
	{
		h = sip_message_get_header_by_name(reply, "Require");
		if (!h || (0 != cstrcasecmp(h, "100rel") && 0 != cstrcasecmp(h, "precondition")))
		{
			assert(0);
			return NULL;
		}

		assert(dialog);
		//struct sip_uac_transaction_t* prack = sip_uac_prack(ctx->sip, dialog, sip_uac_onprack, ctx);
		//sip_uac_add_header(prack, "Supported", "precondition");
		//sip_uac_send(prack, sdp, 0, transport, param);
		return NULL;
	}
	else if (code >= 300)
	{
		// TODO: delete session
		printf("sip_uac_oninvited code: %d\n", code);
		return NULL;
	}
	else
	{
		// trying
		return NULL;
	}
}

static void sip_uac_invite_test(struct sip_uac_test2_t *ctx)
{
    char buffer[1024];
    struct sip_uac_transaction_t* t;
    t = sip_uac_invite(ctx->sip, SIP_FROM, SIP_PEER, sip_uac_oninvited, ctx);
	if (HTTP_AUTHENTICATION_DIGEST == ctx->auth.scheme)
	{
		++ctx->auth.nc;
		snprintf(ctx->auth.uri, sizeof(ctx->auth.uri), "%s", SIP_PEER);
		snprintf(ctx->auth.username, sizeof(ctx->auth.username), "%s", ctx->usr);
		http_header_auth(&ctx->auth, SIP_PWD, "INVITE", NULL, 0, buffer, sizeof(buffer));
		sip_uac_add_header(t, "Proxy-Authorization", buffer);
	}

	struct ice_transport_handler_t handler = {
		ice_transport_ondata,
		sip_uac_ice_transport_onbind,
		ice_transport_onconnected,
	};
	std::shared_ptr<sip_uac_test2_session_t> s(new sip_uac_test2_session_t());
	s->pkts = std::shared_ptr<AVPacketQueue>(new AVPacketQueue(200));
	s->avt = ice_transport_create(0, &handler, s.get());
	memset(&s->audio, 0, sizeof(s->audio));
	memset(&s->video, 0, sizeof(s->video));
	s->user = ctx->usr;
	s->tuac = t;

	{
		AutoThreadLocker locker(ctx->locker);
		if (ctx->sessions.end() != ctx->sessions.find(SIP_PEER))
			return; // ignore
		ctx->sessions.insert(std::make_pair(SIP_PEER, s));
	}

	s->audio.stream = 0;
	s->video.stream = 1;
	s->transport.udp = ctx->udp;
	s->transport.tcp = ctx->tcp;
	struct sockaddr_storage stun;
	memset(&stun, 0, sizeof(stun));
	assert(0 == socket_addr_from_ipv4((struct sockaddr_in*)&stun, TURN_SERVER, STUN_PORT));
	ice_transport_bind(s->avt, 2, 2, TURN_SERVER ? (struct sockaddr*)&stun : NULL, 0, TURN_USR, TURN_PWD);
}

static void sip_uac_register_test(struct sip_uac_test2_t *test);
static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	const cstring_t* h;
	if (0 == test->callid[0] && reply)
	{
		h = sip_message_get_header_by_name(reply, "Call-ID");
		if (h)
		{
			snprintf(test->callid, sizeof(test->callid), "%.*s", (int)h->n, h->p);
			h = sip_message_get_header_by_name(reply, "CSeq");
			test->cseq = atoi(h->p);
		}
	}

	if (200 <= code && code < 300)
	{
		printf("Register OK\n");
		//sip_uac_invite_test(test);
	}
	else if (401 == code)
	{
		// Unauthorized
		memset(&test->auth, 0, sizeof(test->auth));
		h = sip_message_get_header_by_name(reply, "WWW-Authenticate");
		assert(0 == http_header_www_authenticate(h->p, &test->auth));
		test->nonce_count = 0;
		switch (test->auth.scheme)
		{
		case HTTP_AUTHENTICATION_DIGEST:
			sip_uac_register_test(test);
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
	char buffer[256];
	struct sip_uac_transaction_t* t;
	//t = sip_uac_register(uac, "Bob <sip:bob@biloxi.com>", "sip:registrar.biloxi.com", 7200, sip_uac_message_onregister, test);
	t = sip_uac_register(test->sip, SIP_FROM, "sip:" SIP_HOST, SIP_EXPIRED, sip_uac_onregister, test);

	if (test->callid[0])
	{
		// All registrations from a UAC SHOULD use the same Call-ID
		sip_uac_add_header(t, "Call-ID", test->callid);

		snprintf(buffer, sizeof(buffer), "%u REGISTER", ++test->cseq);
		sip_uac_add_header(t, "CSeq", buffer);
	}

	if (HTTP_AUTHENTICATION_DIGEST == test->auth.scheme)
	{
		// https://blog.csdn.net/yunlianglinfeng/article/details/81109380
		// http://www.voidcn.com/article/p-oqqbqgvd-bgn.html
		++test->auth.nc;
		snprintf(test->auth.uri, sizeof(test->auth.uri), "sip:%s", SIP_HOST);
		snprintf(test->auth.username, sizeof(test->auth.username), "%s", test->usr);
		http_header_auth(&test->auth, SIP_PWD, "REGISTER", NULL, 0, buffer, sizeof(buffer));
		sip_uac_add_header(t, "Authorization", buffer);
	}

	struct sip_transport_t transport = {
			sip_uac_transport_via,
			sip_uac_transport_send,
	};
	assert(0 == sip_uac_send(t, NULL, 0, &transport, &test->transport));
}

static int STDCALL TimerThread(void* param)
{
	uint64_t clock = 0;
	struct sip_uac_test2_t* ctx = (struct sip_uac_test2_t*)param;
	
	while (ctx->running)
	{
		uint64_t now = time64_now();
		if (clock + SIP_EXPIRED * 1000 < now)
		{
			clock = now;
			sip_uac_register_test(ctx);
		}

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

	request = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	response = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);

    do
    {
		socklen_t addrlen;
		struct sockaddr_storage addr;
		addrlen = sizeof(addr);
		memset(buffer, 0, sizeof(buffer));
		int r = socket_recvfrom(socket_invalid==test->tcp ? test->udp : test->tcp, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
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

static void* sip_uas_onsubscribe(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, sip_subscribe_t* subscribe)
{
	return NULL;
}
static int sip_uas_onnotify(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const struct cstring_t* event)
{
	return 0;
}
static int sip_uas_onpublish(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const struct cstring_t* event)
{
	return 0;
}

static int sip_uas_oninfo(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return 0;
}

static int sip_uas_onrefer(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return 0;
}

static int sip_uas_onprack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	return 0;
}

static int sip_uas_onupdate(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	return 0;
}

void sip_uac_test2(void)
{
	socket_init();
	struct sip_uac_test2_t test;
	test.running = true;
	test.callid[0] = 0;
    struct sip_uas_handler_t handler = {
		sip_uas_transport_send,
		sip_uas_onregister,
		sip_uas_oninvite,
		sip_uas_onack,
		sip_uas_onprack,
		sip_uas_onupdate,
		sip_uas_onbye,
		sip_uas_oncancel,
		sip_uas_onsubscribe,
		sip_uas_onnotify,
		sip_uas_onpublish,
		sip_uas_oninfo,
		sip_uas_onmessage,
		sip_uas_onrefer,
	};

	assert(1 == sscanf(SIP_FROM, "sip:%[^@]", test.usr));
	ip_route_get(SIP_HOST, test.local);
	
	test.udp = socket_udp();
	//test.tcp = socket_connect_host(SIP_HOST, SIP_PORT, 2000);
	//if(socket_invalid != test.tcp)
	//	socket_setnonblock(test.tcp, 0);
	test.tcp = socket_invalid;
	test.sip = sip_agent_create(&handler, &test);
    socket_bind_any(test.udp, SIP_PORT);
	test.transport.udp = test.udp;
	test.transport.tcp = test.tcp;

	pthread_t th;
	thread_create(&th, TimerThread, &test);

	//sip_uac_register_test(&test);
    sip_uac_test_process(&test);

	thread_destroy(th);
	sip_agent_destroy(test.sip);
	socket_close(test.udp);
	socket_cleanup();
}
