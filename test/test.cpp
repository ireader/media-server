#include "sys/sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <string>
#include <vector>

#include "Reflector.h"

#define RE_REGISTER(name, proto, func) static bool b_##name = Reflector::Instance()->registerFun(#name, proto, &func)

#define RE_RUN_REG(name,argc,argv) Reflector::Instance()->runFun(name, argc, argv)

#define RE_GET_REG Reflector::Instance()->getAllRegisterFun

static std::string str_register_fun;
#define T_RE_GET_ALL_REG() do{\
    std::vector<std::string> regFuncVec = RE_GET_REG();\
    for(auto& x: regFuncVec){\
        str_register_fun += "\t";\
        str_register_fun += x;\
        str_register_fun += "\n";\
    }\
}while(0)

void usage(int argc, char const *argv[]){
    printf( "****************************************\n");
    printf( "usage: \n"                                 );
    printf( "./test -c <func name> \n"	                );
    printf( "useful func: \n"                           );
    printf( "%s", str_register_fun.c_str()              );
    printf( "****************************************\n");
}

/** 
 * tool 
 * 由于测试项函数参数不统一，这里做一些工具，用于实现反射机制.
 * TODO：测试项函数参数格式统一
*/

/* 用于套壳调用函数，参数 void */
#define DEF_FUN_VOID2(name, ...) void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        name();return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" #__VA_ARGS__ ")", t_##name)
#define DEF_FUN_VOID(name) DEF_FUN_VOID2(name, void)

/* 用于套壳调用函数，参数 char* */
#define DEF_FUN_PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(4 != argc) return -1;\
        name(argv[3]);return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" #__VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, char* */
#define DEF_FUN_2PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(5 != argc) return -1;\
        name(argv[3], argv[4]);return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" #__VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, char*, char*, char* */
#define DEF_FUN_4PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(7 != argc) return -1;\
        name(argv[3], argv[4], argv[5], argv[6]);return 0;\
    } \
    RE_REGISTER(name, "void " #name  "(" #__VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, int */
#define DEF_FUN_PCHAR_INT(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(5 != argc) return -1;\
        name(argv[3], (int)atoi(argv[4]));return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" #__VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 int, int, char*, char* */
#define DEF_FUN_INT_INT_PCHAR_PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(7 != argc) return -1;\
        name((int)atoi(argv[3]), (int)atoi(argv[4]), argv[5], argv[6]);return 0;\
    }\
     RE_REGISTER(name, "void " #name "(" #__VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, int, int, char* */
#define DEF_FUN_PCHAR_INT_INT_PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(7 != argc) return -1;\
        name(argv[3], (int)atoi(argv[4]), (int)atoi(argv[5]), argv[6]);return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" # __VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, int, char* */
#define DEF_FUN_PCHAR_INT_PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(6 != argc) return -1;\
        name(argv[3], (int)atoi(argv[4]), argv[5]);return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" # __VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 char*, int, char*,int, int */
#define DEF_FUN_PCHAR_INT_PCHAR_INT_INT(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(8 != argc) return -1;\
        name(argv[3], (int)atoi(argv[4]), argv[5], (int)atoi(argv[6]), (int)atoi(argv[7]));return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" # __VA_ARGS__ ")", t_##name)

/* 用于套壳调用函数，参数 int, const char*, uint16_t, uint32_t, const char* */
#define DEF_FUN_INT_PCHAR_INT_INT_PCHAR(name, ...)  void name ( __VA_ARGS__ ); \
    int t_##name(int argc, char const *argv[]){\
        if(8 != argc) return -1;\
        name((int)atoi(argv[3]), argv[4], (uint16_t)atoi(argv[5]), (uint32_t)atoi(argv[6]), argv[7]);return 0;\
    } \
    RE_REGISTER(name, "void " #name "(" # __VA_ARGS__ ")", t_##name)

extern "C" DEF_FUN_VOID(amf0_test);
extern "C" DEF_FUN_VOID(rtp_queue_test);
extern "C" DEF_FUN_VOID(mpeg4_aac_test);
extern "C" DEF_FUN_VOID(mpeg4_avc_test);
extern "C" DEF_FUN_VOID(mpeg4_hevc_test);
extern "C" DEF_FUN_VOID(mpeg4_vvc_test);
extern "C" DEF_FUN_VOID(mp3_header_test);
extern "C" DEF_FUN_VOID(h264_mp4toannexb_test);
extern "C" DEF_FUN_VOID(sdp_a_fmtp_test);
extern "C" DEF_FUN_VOID(sdp_a_rtpmap_test);
extern "C" DEF_FUN_VOID(sdp_a_webrtc_test);
extern "C" DEF_FUN_VOID(rtsp_client_auth_test);
extern "C" DEF_FUN_VOID(rtsp_header_range_test);
extern "C" DEF_FUN_VOID(rtsp_header_rtp_info_test);
extern "C" DEF_FUN_VOID(rtsp_header_transport_test);
extern "C" DEF_FUN_VOID(http_header_host_test);
extern "C" DEF_FUN_VOID(http_header_content_type_test);
extern "C" DEF_FUN_VOID(http_header_authorization_test);
extern "C" DEF_FUN_VOID(http_header_www_authenticate_test);
extern "C" DEF_FUN_VOID(http_header_auth_test);

extern "C" DEF_FUN_VOID(rtsp_example);
extern "C" DEF_FUN_VOID(rtsp_push_server);
extern "C" DEF_FUN_2PCHAR(rtsp_client_test, const char* host, const char* file);
DEF_FUN_INT_PCHAR_INT_INT_PCHAR(rstp_demuxer_test, int payload, const char* encoding, uint16_t seq, uint32_t ssrc, const char* rtpfile);
DEF_FUN_2PCHAR(rtsp_client_push_test, const char* host, const char* file);
DEF_FUN_PCHAR(rtsp_client_input_test, const char* file);
DEF_FUN_PCHAR(rtp_dump_test, const char* file);
//DEF_FUN_PCHAR(rtp_header_ext_test, const char* rtpfile);
DEF_FUN_VOID(rtp_payload_test);

DEF_FUN_PCHAR(flv_parser_test, const char* flv);
DEF_FUN_PCHAR(flv_read_write_test, const char* flv);
DEF_FUN_2PCHAR(flv2ts_test, const char* inputFLV, const char* outputTS);
DEF_FUN_2PCHAR(ts2flv_test, const char* inputTS, const char* outputFLV);
DEF_FUN_2PCHAR(avc2flv_test, const char* inputH264, const char* outputFLV);
DEF_FUN_2PCHAR(hevc2flv_test, const char* inputH265, const char* outputFLV);
DEF_FUN_PCHAR(flv_reader_test, const char* file);
DEF_FUN_2PCHAR(av1toflv_test, const char* obu, const char* outputFLV);
DEF_FUN_PCHAR(av1_rtp_test, const char* low_overhead_bitstream_format_obu);
extern "C" DEF_FUN_PCHAR(aom_av1_obu_test, const char* file);

DEF_FUN_PCHAR(mov_2_flv_test, const char* mp4);
DEF_FUN_PCHAR(mov_reader_test, const char* mp4);
DEF_FUN_INT_INT_PCHAR_PCHAR(mov_writer_test, int w, int h, const char* inflv, const char* outmp4);
DEF_FUN_INT_INT_PCHAR_PCHAR(fmp4_writer_test, int w, int h, const char* inflv, const char* outmp4);
DEF_FUN_PCHAR_INT_INT_PCHAR(mov_writer_h264, const char* h264, int width, int height, const char* mp4);
DEF_FUN_PCHAR_INT_INT_PCHAR(mov_writer_h265, const char* h265, int width, int height, const char* mp4);
DEF_FUN_PCHAR_INT_INT_PCHAR(mov_writer_av1, const char* obu, int width, int height, const char* mp4);
DEF_FUN_PCHAR_INT_PCHAR(mov_writer_audio, const char* audio, int type, const char* mp4);
DEF_FUN_2PCHAR(fmp4_writer_test2, const char* mp4, const char* outmp4);
DEF_FUN_PCHAR(mov_rtp_test, const char* mp4);

DEF_FUN_PCHAR(mpeg_ts_dec_test, const char* file);
DEF_FUN_PCHAR(mpeg_ts_test, const char* input);
DEF_FUN_PCHAR(mpeg_ps_test, const char* input);
DEF_FUN_PCHAR(mpeg_ps_2_flv_test, const char* ps);
DEF_FUN_PCHAR(flv_2_mpeg_ps_test, const char* flv);
DEF_FUN_PCHAR(mpeg_ps_dec_test, const char* file);

extern "C" DEF_FUN_PCHAR_INT(http_server_test, const char* ip, int port);
DEF_FUN_PCHAR_INT_PCHAR_INT_INT(dash_dynamic_test, const char* ip, int port, const char* file, int width, int height);
DEF_FUN_2PCHAR(dash_static_test, const char* mp4, const char* name);
DEF_FUN_PCHAR_INT(hls_server_test, const char* ip, int port);
DEF_FUN_PCHAR(hls_segmenter_flv, const char* file);
#if defined(_HAVE_FFMPEG_)
DEF_FUN_PCHAR(hls_segmenter_fmp4_test, const char* file);
#endif

DEF_FUN_4PCHAR(rtmp_play_test, const char* host, const char* app, const char* stream, const char* flv);
DEF_FUN_4PCHAR(rtmp_publish_test, const char* host, const char* app, const char* stream, const char* flv);
DEF_FUN_4PCHAR(rtmp_play_aio_test, const char* host, const char* app, const char* stream, const char* file);
DEF_FUN_4PCHAR(rtmp_publish_aio_test, const char* host, const char* app, const char* stream, const char* file);
DEF_FUN_PCHAR(rtmp_server_vod_test, const char* flv);
DEF_FUN_PCHAR(rtmp_server_publish_test, const char* flv);
DEF_FUN_PCHAR(rtmp_server_vod_aio_test, const char* flv);
DEF_FUN_PCHAR(rtmp_server_publish_aio_test, const char* flv);
DEF_FUN_PCHAR_INT(rtmp_server_forward_aio_test, const char* ip, int port);
DEF_FUN_PCHAR(rtmp_server_input_test, const char* file);
DEF_FUN_PCHAR(rtmp_input_test, const char* file);

extern "C" DEF_FUN_VOID(sip_header_test);
extern "C" DEF_FUN_VOID(sip_agent_test);
DEF_FUN_VOID(sip_uac_message_test);
DEF_FUN_VOID(sip_uas_message_test);
DEF_FUN_VOID(sip_uac_test);
DEF_FUN_VOID(sip_uas_test);
DEF_FUN_VOID(sip_uac_test2);
DEF_FUN_VOID(sip_uas_test2);

DEF_FUN_PCHAR(sdp_test, const char* file);

int binnary_diff(const char* file1, const char* file2);

int main(int argc, const char* argv[])
{
    T_RE_GET_ALL_REG();
    
    socket_init();

    if(argc < 3){
        usage(argc, argv);

        printf("run default test\n");
        printf( "****************************************\n");
        RE_RUN_REG("amf0_test", argc, argv);
        RE_RUN_REG("rtp_queue_test", argc, argv);
        RE_RUN_REG("mpeg4_aac_test", argc, argv);
        RE_RUN_REG("mpeg4_avc_test", argc, argv);
        RE_RUN_REG("mpeg4_hevc_test", argc, argv);
        RE_RUN_REG("mpeg4_vvc_test", argc, argv);
        RE_RUN_REG("mp3_header_test", argc, argv);
        RE_RUN_REG("h264_mp4toannexb_test", argc, argv);
        RE_RUN_REG("sdp_a_fmtp_test", argc, argv);
        RE_RUN_REG("sdp_a_rtpmap_test", argc, argv);
        RE_RUN_REG("sdp_a_webrtc_test", argc, argv);
        RE_RUN_REG("rtsp_header_range_test", argc, argv);
        RE_RUN_REG("rtsp_header_rtp_info_test", argc, argv);
        RE_RUN_REG("rtsp_header_transport_test", argc, argv);
        RE_RUN_REG("http_header_host_test", argc, argv);
        RE_RUN_REG("http_header_auth_test", argc, argv);
        RE_RUN_REG("http_header_content_type_test", argc, argv);
        RE_RUN_REG("http_header_authorization_test", argc, argv);
        RE_RUN_REG("http_header_www_authenticate_test", argc, argv);
        RE_RUN_REG("rtsp_client_auth_test", argc, argv);
        RE_RUN_REG("sip_header_test", argc, argv);
        RE_RUN_REG("sip_uac_message_test", argc, argv);
        RE_RUN_REG("sip_uas_message_test", argc, argv);
        goto EXIT;
    }

    printf("run %s\n", argv[2]);
    RE_RUN_REG(argv[2], argc, argv);

EXIT:
	socket_cleanup();
	return 0;
}
