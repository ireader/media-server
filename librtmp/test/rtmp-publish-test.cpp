#include "sockutil.h"
#include "sys/system.h"
#include "rtmp-client.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//#define CORRUPT_RTMP_CHUNK_DATA

#if defined(CORRUPT_RTMP_CHUNK_DATA)
static void rtmp_corrupt_data(const void* data, size_t bytes)
{
    static unsigned int seed;
    if (0 == seed)
    {
        seed = (unsigned int)time(NULL);
        srand(seed);
    }

    if (bytes < 1)
        return;

    //size_t i = bytes > 20 ? 20 : bytes;
    //i = rand() % i;

    //uint8_t v = ((uint8_t*)data)[i];
    //((uint8_t*)data)[i] = rand() % 255;
    //printf("rtmp_corrupt_data[%d] %d == %d\n", i, (int)v, (int)((uint8_t*)data)[i]);
 
    if (5 == rand() % 10)
    {
        size_t i = rand() % bytes;
        uint8_t v = ((uint8_t*)data)[i];
        ((uint8_t*)data)[i] = rand() % 255;
        printf("rtmp_corrupt_data[%d] %d == %d\n", i, (int)v, (int)((uint8_t*)data)[i]);
    }
}

static uint8_t s_buffer[4 * 1024 * 1024];
static size_t s_offset;
static FILE* s_fp;
static void fwritepacket(uint32_t timestamp)
{
    assert(4 == fwrite(&s_offset, 1, 4, s_fp));
    assert(4 == fwrite(&timestamp, 1, 4, s_fp));
    assert(s_offset == fwrite(s_buffer, 1, s_offset, s_fp));
    s_offset = 0;
}
#endif

static int rtmp_client_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, (void*)header, len);
	socket_setbufvec(vec, 1, (void*)data, bytes);

#if defined(CORRUPT_RTMP_CHUNK_DATA)
	//if (len > 0)
	//{
	//    assert(s_offset + len < sizeof(s_buffer));
	//    memcpy(s_buffer + s_offset, header, len);
	//    s_offset += len;
	//}
	//if (bytes > 0)
	//{
	//    assert(s_offset + bytes < sizeof(s_buffer));
	//    memcpy(s_buffer + s_offset, data, bytes);
	//    s_offset += bytes;
	//}
	
	rtmp_corrupt_data(header, len);
    rtmp_corrupt_data(data, bytes);
#endif
	return socket_send_v_all_by_time(*socket, vec, bytes > 0 ? 2 : 1, 0, 5000);
}

static void rtmp_client_push(const char* flv, rtmp_client_t* rtmp)
{
	int r, type;
    int avcrecord = 0;
    int aacconfig = 0;
	uint32_t timestamp;
	uint32_t s_timestamp = 0;
	uint32_t diff = 0;
	uint64_t clock;
	
	static char packet[2 * 1024 * 1024];
	while (1)
	{
		void* f = flv_reader_create(flv);

		clock = system_clock(); // timestamp start from 0
		while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
		{
			uint64_t t = system_clock();
			if (clock + timestamp > t && clock + timestamp < t + 3 * 1000) // dts skip
				system_sleep(clock + timestamp - t);
			else if (clock + timestamp > t + 3 * 1000)
				clock = t - timestamp;
			
			timestamp += diff;
			s_timestamp = timestamp > s_timestamp ? timestamp : s_timestamp;

			if (FLV_TYPE_AUDIO == type)
			{
                if (0 == packet[1])
                {
                    if(0 != aacconfig)
                        continue;
                    aacconfig = 1;
                }
				r = rtmp_client_push_audio(rtmp, packet, r, timestamp);
			}
			else if (FLV_TYPE_VIDEO == type)
			{
                if (0 == packet[1] || 2 == packet[1])
                {
                    if (0 != avcrecord)
                        continue;
                    avcrecord = 1;
                }
				//bool keyframe = 1 == ((packet[0] & 0xF0) >> 4);
				//printf("timestamp: %u, s_timestamp: %u%s\n", timestamp, s_timestamp, keyframe ? " [I]" : "");
				//if (timestamp > 10 * 1000 && keyframe)
				//{
				//	uint8_t header[5];
				//	header[0] = (1 << 4) /* FrameType */ | 7 /* AVC */;
				//	header[1] = 2; // AVC end of sequence
				//	header[2] = 0;
				//	header[3] = 0;
				//	header[4] = 0;
				//	r = rtmp_client_push_video(rtmp, header, 5, timestamp);
				//	system_sleep(600 * 1000);
				//}
				r = rtmp_client_push_video(rtmp, packet, r, timestamp);
			}
			else if (FLV_TYPE_SCRIPT == type)
			{
				r = rtmp_client_push_script(rtmp, packet, r, timestamp);
			}
			else
			{
				assert(0);
				r = 0; // ignore
			}

			if (0 != r)
			{
				assert(0);
				break; // TODO: handle send failed
			}
		}

		flv_reader_destroy(f);

		diff = s_timestamp + 30;
	}

EXIT:
	return;
}

// rtmp://video-center.alivecdn.com/live/hello?vhost=your.domain
// rtmp_publish_test("video-center.alivecdn.com", "live", "hello?vhost=your.domain", local-flv-file-name)
void rtmp_publish_test(const char* host, const char* app, const char* stream, const char* flv)
{
	static char packet[2 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s", host, app); // tcurl

	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_client_send;

	socket_init();
	socket_t socket = socket_connect_host(host, 1935, 2000);
	socket_setnonblock(socket, 0);

	rtmp_client_t* rtmp = rtmp_client_create(app, stream, packet/*tcurl*/, &socket, &handler);
	int r = rtmp_client_start(rtmp, 0);

	while (4 != rtmp_client_getstate(rtmp) && (r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		assert(0 == rtmp_client_input(rtmp, packet, r));
	}

	rtmp_client_push(flv, rtmp);

	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
