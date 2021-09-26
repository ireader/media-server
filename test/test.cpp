#include "sys/sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <string>
#include <vector>

#include "Reflector.h"

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
#define DEF_FUN_VOID(name) int t_##name(int argc, char const *argv[]){\
        name();return 0;\
    }
/* 用于套壳调用函数，参数 char* */
#define DEF_FUN_PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(2 != argc) return -1;\
        name(argv[1]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, char* */
#define DEF_FUN_2PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(3 != argc) return -1;\
        name(argv[1], argv[2]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, char*, char*, char* */
#define DEF_FUN_4PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(5 != argc) return -1;\
        name(argv[1], argv[2], argv[3], argv[4]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, int */
#define DEF_FUN_PCHAR_INT(name) int t_##name(int argc, char const *argv[]){\
        if(3 != argc) return -1;\
        name(argv[1], (int)atoi(argv[2]));return 0;\
    }
/* 用于套壳调用函数，参数 int, int, char*, char* */
#define DEF_FUN_INT_INT_PCHAR_PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(5 != argc) return -1;\
        name((int)atoi(argv[1]), (int)atoi(argv[2]), argv[3], argv[4]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, int, int, char* */
#define DEF_FUN_PCHAR_INT_INT_PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(5 != argc) return -1;\
        name(argv[1], (int)atoi(argv[2]), (int)atoi(argv[3]), argv[4]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, int, char* */
#define DEF_FUN_PCHAR_INT_PCHAR(name) int t_##name(int argc, char const *argv[]){\
        if(4 != argc) return -1;\
        name(argv[1], (int)atoi(argv[2]), argv[3]);return 0;\
    }
/* 用于套壳调用函数，参数 char*, int, char*,int, int */
#define DEF_FUN_PCHAR_INT_PCHAR_INT_INT(name) int t_##name(int argc, char const *argv[]){\
        if(6 != argc) return -1;\
        name(argv[1], (int)atoi(argv[2]), argv[3], (int)atoi(argv[4]), (int)atoi(argv[5]));return 0;\
    }

#define T_RE_REGISTER(name) RE_REGISTER_SETNAME(name, t_##name)

extern "C" void amf0_test(void);
DEF_FUN_VOID(amf0_test);

extern "C" void rtp_queue_test(void);
DEF_FUN_VOID(rtp_queue_test);

extern "C" void mpeg4_aac_test(void);
DEF_FUN_VOID(mpeg4_aac_test);

extern "C" void mpeg4_avc_test(void);
DEF_FUN_VOID(mpeg4_avc_test);

extern "C" void mpeg4_hevc_test(void);
DEF_FUN_VOID(mpeg4_hevc_test);

extern "C" void mp3_header_test(void);
DEF_FUN_VOID(mp3_header_test);

extern "C" void sdp_a_fmtp_test(void);
DEF_FUN_VOID(sdp_a_fmtp_test);

extern "C" void sdp_a_rtpmap_test(void);
DEF_FUN_VOID(sdp_a_rtpmap_test);

extern "C" void rtsp_client_auth_test(void);
DEF_FUN_VOID(rtsp_client_auth_test);

extern "C" void rtsp_header_range_test(void);
DEF_FUN_VOID(rtsp_header_range_test);

extern "C" void rtsp_header_rtp_info_test(void);
DEF_FUN_VOID(rtsp_header_rtp_info_test);

extern "C" void rtsp_header_transport_test(void);
DEF_FUN_VOID(rtsp_header_transport_test);

extern "C" void http_header_host_test(void);
DEF_FUN_VOID(http_header_host_test);

extern "C" void http_header_content_type_test(void);
DEF_FUN_VOID(http_header_content_type_test);

extern "C" void http_header_authorization_test(void);
DEF_FUN_VOID(http_header_authorization_test);

extern "C" void http_header_www_authenticate_test(void);
DEF_FUN_VOID(http_header_www_authenticate_test);

extern "C" void http_header_auth_test(void);
DEF_FUN_VOID(http_header_auth_test);

extern "C" void rtsp_example();
DEF_FUN_VOID(rtsp_example);

extern "C" void rtsp_push_server();
DEF_FUN_VOID(rtsp_push_server);

extern "C" void rtsp_client_test(const char* host, const char* file);
DEF_FUN_2PCHAR(rtsp_client_test);

extern "C" void http_server_test(const char* ip, int port);
DEF_FUN_PCHAR_INT(http_server_test);

void rtp_payload_test();
DEF_FUN_VOID(rtp_payload_test);

void mpeg_ts_dec_test(const char* file);
DEF_FUN_PCHAR(mpeg_ts_dec_test);

void mpeg_ts_test(const char* input);
DEF_FUN_PCHAR(mpeg_ts_test);

void mpeg_ps_test(const char* input);
DEF_FUN_PCHAR(mpeg_ps_test);

void flv_2_mpeg_ps_test(const char* flv);
DEF_FUN_PCHAR(flv_2_mpeg_ps_test);

void mpeg_ps_dec_test(const char* file);
DEF_FUN_PCHAR(mpeg_ps_dec_test);

void flv_read_write_test(const char* flv);
DEF_FUN_PCHAR(flv_read_write_test);

void flv2ts_test(const char* inputFLV, const char* outputTS);
DEF_FUN_2PCHAR(flv2ts_test);

void ts2flv_test(const char* inputTS, const char* outputFLV);
DEF_FUN_2PCHAR(ts2flv_test);

void avc2flv_test(const char* inputH264, const char* outputFLV);
DEF_FUN_2PCHAR(avc2flv_test);

void hevc2flv_test(const char* inputH265, const char* outputFLV);
DEF_FUN_2PCHAR(hevc2flv_test);

void flv_reader_test(const char* file);
DEF_FUN_PCHAR(flv_reader_test);

void mov_2_flv_test(const char* mp4);
DEF_FUN_PCHAR(mov_2_flv_test);

void mov_reader_test(const char* mp4);
DEF_FUN_PCHAR(mov_reader_test);

void mov_writer_test(int w, int h, const char* inflv, const char* outmp4);
DEF_FUN_INT_INT_PCHAR_PCHAR(mov_writer_test);

void fmp4_writer_test(int w, int h, const char* inflv, const char* outmp4);
DEF_FUN_INT_INT_PCHAR_PCHAR(fmp4_writer_test);

void mov_writer_h264(const char* h264, int width, int height, const char* mp4);
DEF_FUN_PCHAR_INT_INT_PCHAR(mov_writer_h264);

void mov_writer_h265(const char* h265, int width, int height, const char* mp4);
DEF_FUN_PCHAR_INT_INT_PCHAR(mov_writer_h265);

void mov_writer_audio(const char* audio, int type, const char* mp4);
DEF_FUN_PCHAR_INT_PCHAR(mov_writer_audio);

void hls_segmenter_flv(const char* file);
DEF_FUN_PCHAR(hls_segmenter_flv);

#if defined(_HAVE_FFMPEG_)
void hls_segmenter_fmp4_test(const char* file);
DEF_FUN_PCHAR(hls_segmenter_fmp4_test);
#endif

void hls_server_test(const char* ip, int port);
DEF_FUN_PCHAR_INT(hls_server_test);

void dash_dynamic_test(const char* ip, int port, const char* file, int width, int height);
DEF_FUN_PCHAR_INT_PCHAR_INT_INT(dash_dynamic_test);

void dash_static_test(const char* mp4, const char* name);
DEF_FUN_2PCHAR(dash_static_test);

void rtmp_play_test(const char* host, const char* app, const char* stream, const char* flv);
DEF_FUN_4PCHAR(rtmp_play_test);

void rtmp_publish_test(const char* host, const char* app, const char* stream, const char* flv);
DEF_FUN_4PCHAR(rtmp_publish_test);

void rtmp_play_aio_test(const char* host, const char* app, const char* stream, const char* file);
DEF_FUN_4PCHAR(rtmp_play_aio_test);

void rtmp_publish_aio_test(const char* host, const char* app, const char* stream, const char* file);
DEF_FUN_4PCHAR(rtmp_publish_aio_test);

void rtmp_server_vod_test(const char* flv);
DEF_FUN_PCHAR(rtmp_server_vod_test);

void rtmp_server_publish_test(const char* flv);
DEF_FUN_PCHAR(rtmp_server_publish_test);

void rtmp_server_vod_aio_test(const char* flv);
DEF_FUN_PCHAR(rtmp_server_vod_aio_test);

void rtmp_server_publish_aio_test(const char* flv);
DEF_FUN_PCHAR(rtmp_server_publish_aio_test);

void rtmp_server_forward_aio_test(const char* ip, int port);
DEF_FUN_PCHAR_INT(rtmp_server_forward_aio_test);

extern "C" void sip_header_test(void);
DEF_FUN_VOID(sip_header_test);

extern "C" void sip_agent_test(void);
DEF_FUN_VOID(sip_agent_test);

void sip_uac_message_test(void);
DEF_FUN_VOID(sip_uac_message_test);

void sip_uas_message_test(void);
DEF_FUN_VOID(sip_uas_message_test);

void sip_uac_test(void);
DEF_FUN_VOID(sip_uac_test);

void sip_uas_test(void);
DEF_FUN_VOID(sip_uas_test);

void sip_uac_test2(void);
DEF_FUN_VOID(sip_uac_test2);

void sip_uas_test2(void);
DEF_FUN_VOID(sip_uas_test2);

int binnary_diff(const char* file1, const char* file2);

int main(int argc, const char* argv[])
{
    T_RE_REGISTER(amf0_test);
    T_RE_REGISTER(rtp_queue_test);
    T_RE_REGISTER(mpeg4_aac_test);
    T_RE_REGISTER(mpeg4_avc_test);
    T_RE_REGISTER(mpeg4_hevc_test);
    T_RE_REGISTER(mp3_header_test);
    T_RE_REGISTER(sdp_a_fmtp_test);
    T_RE_REGISTER(sdp_a_rtpmap_test);
    T_RE_REGISTER(rtsp_client_auth_test);
    T_RE_REGISTER(rtsp_header_range_test);
    T_RE_REGISTER(rtsp_header_rtp_info_test);
    T_RE_REGISTER(rtsp_header_transport_test);
    T_RE_REGISTER(http_header_host_test);
    T_RE_REGISTER(http_header_content_type_test);
    T_RE_REGISTER(http_header_authorization_test);
    T_RE_REGISTER(http_header_www_authenticate_test);
    T_RE_REGISTER(http_header_auth_test);
    T_RE_REGISTER(rtsp_example);
    T_RE_REGISTER(rtsp_push_server);
    T_RE_REGISTER(rtsp_client_test);
    T_RE_REGISTER(http_server_test);
    T_RE_REGISTER(rtp_payload_test);
    T_RE_REGISTER(mpeg_ts_dec_test);
    T_RE_REGISTER(mpeg_ts_test);
    T_RE_REGISTER(mpeg_ps_test);
    T_RE_REGISTER(flv_2_mpeg_ps_test);
    T_RE_REGISTER(mpeg_ps_dec_test);
    T_RE_REGISTER(flv_read_write_test);
    T_RE_REGISTER(flv2ts_test);
    T_RE_REGISTER(ts2flv_test);
    T_RE_REGISTER(avc2flv_test);
    T_RE_REGISTER(hevc2flv_test);
    T_RE_REGISTER(flv_reader_test);
    T_RE_REGISTER(mov_2_flv_test);
    T_RE_REGISTER(mov_reader_test);
    T_RE_REGISTER(mov_writer_test);
    T_RE_REGISTER(fmp4_writer_test);
    T_RE_REGISTER(mov_writer_h264);
    T_RE_REGISTER(mov_writer_h265);
    T_RE_REGISTER(mov_writer_audio);
    T_RE_REGISTER(hls_segmenter_flv);
    #if defined(_HAVE_FFMPEG_)
    T_RE_REGISTER(hls_segmenter_fmp4_test);
    #endif
    T_RE_REGISTER(hls_server_test);
    T_RE_REGISTER(dash_dynamic_test);
    T_RE_REGISTER(dash_static_test);
    T_RE_REGISTER(rtmp_play_test);
    T_RE_REGISTER(rtmp_publish_test);
    T_RE_REGISTER(rtmp_play_aio_test);
    T_RE_REGISTER(rtmp_publish_aio_test);
    T_RE_REGISTER(rtmp_server_vod_test);
    T_RE_REGISTER(rtmp_server_publish_test);
    T_RE_REGISTER(rtmp_server_vod_aio_test);
    T_RE_REGISTER(rtmp_server_publish_aio_test);
    T_RE_REGISTER(rtmp_server_forward_aio_test);
    T_RE_REGISTER(sip_header_test);
    T_RE_REGISTER(sip_agent_test);
    T_RE_REGISTER(sip_uac_message_test);
    T_RE_REGISTER(sip_uas_message_test);
    T_RE_REGISTER(sip_uac_test);
    T_RE_REGISTER(sip_uas_test);
    T_RE_REGISTER(sip_uac_test2);
    T_RE_REGISTER(sip_uas_test2);
    
    T_RE_GET_ALL_REG();
    
    std::string runFuncName = argv[2];
    int arc = 0;
    
    socket_init();

    if(3 != argc){
        usage(argc, argv);
        printf("run default test\n");
        printf( "****************************************\n");
        RE_RUN_REG("amf0_test", argc, argv);
        RE_RUN_REG("rtp_queue_test", argc, argv);
        RE_RUN_REG("mpeg4_aac_test", argc, argv);
        RE_RUN_REG("mpeg4_avc_test", argc, argv);
        RE_RUN_REG("mpeg4_hevc_test", argc, argv);
        RE_RUN_REG("mp3_header_test", argc, argv);
        RE_RUN_REG("sdp_a_fmtp_test", argc, argv);
        RE_RUN_REG("sdp_a_rtpmap_test", argc, argv);
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

    printf("run %s\n", runFuncName.c_str());
    
    if(runFuncName == "mpeg_ts_dec_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"fileSequence0.ts";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mpeg_ts_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"hevc_aac.ts";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mpeg_ps_dec_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"sjz.ps";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mpeg_ps_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"sjz.ps";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_2_flv_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_reader_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_writer_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"768";
        arv[2] = (char*)"432";
        arv[3] = (char*)"720p.mp4.flv";
        arv[4] = (char*)"720p.mp4.flv.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_writer_audio"){
        arc = 4; char* arv[4];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.mp4";
        arv[2] = (char*)"1";
        arv[3] = (char*)"aac.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_writer_h264"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.h264";
        arv[2] = (char*)"1280";
        arv[3] = (char*)"720";
        arv[4] = (char*)"720p.h264.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "mov_writer_h265"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.h265";
        arv[2] = (char*)"1280";
        arv[3] = (char*)"720";
        arv[4] = (char*)"720p.h265.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "fmp4_writer_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"1280";
        arv[2] = (char*)"720";
        arv[3] = (char*)"720p.flv";
        arv[4] = (char*)"720p.frag.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "flv_reader_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "flv_read_write_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "ts2flv_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"bipo.ts";
        arv[2] = (char*)"bipo.ts.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "flv2ts_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"bipo.ts.flv";
        arv[2] = (char*)"bipo.ts.flv.ts";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "avc2flv_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"4k.h264";
        arv[2] = (char*)"out.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "hevc2flv_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"BigBuckBunny-3840x2160.h265";
        arv[2] = (char*)"out.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "flv_reader_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"out.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "hls_segmenter_flv"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

#if defined(_HAVE_FFMPEG_)
    if(runFuncName == "hls_segmenter_fmp4_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.mp4";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }
#endif

    if(runFuncName == "dash_dynamic_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = NULL;
        arv[2] = (char*)"80";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "dash_static_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.mp4";
        arv[2] = (char*)"name";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "hls_server_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = NULL;
        arv[2] = (char*)"80";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "http_server_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = NULL;
        arv[2] = (char*)"80";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtsp_client_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"192.168.241.129";
        arv[2] = (char*)"test.rtp";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_play_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"192.168.241.129";
        arv[2] = (char*)"live";
        arv[3] = (char*)"hevc";
        arv[4] = (char*)"h265.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_publish_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"192.168.241.129";
        arv[2] = (char*)"live";
        arv[3] = (char*)"avc";
        arv[4] = (char*)"h264.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_play_aio_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"192.168.241.129";
        arv[2] = (char*)"live";
        arv[3] = (char*)"avc";
        arv[4] = (char*)"avc.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_publish_aio_test"){
        arc = 5; char* arv[5];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"192.168.241.129";
        arv[2] = (char*)"live";
        arv[3] = (char*)"avc";
        arv[4] = (char*)"avc.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_server_publish_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"h265.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_server_vod_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"h264.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_server_vod_aio_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_server_publish_aio_test"){
        arc = 2; char* arv[2];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = (char*)"720p.flv";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    if(runFuncName == "rtmp_server_forward_aio_test"){
        arc = 3; char* arv[3];
        arv[0] = (char*)runFuncName.c_str();
        arv[1] = NULL;
        arv[2] = (char*)"1935";
        RE_RUN_REG(runFuncName.c_str(), arc, (const char**)arv);
        goto EXIT;
    }

    RE_RUN_REG(runFuncName.c_str(), argc, argv);

EXIT:
	socket_cleanup();
	return 0;
}
