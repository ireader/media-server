* Build status: [![Build Status](https://travis-ci.org/ireader/media-server.svg?branch=master)](https://travis-ci.org/ireader/media-server) <a href="https://scan.coverity.com/projects/ireader-media-server"> <img alt="Coverity Scan Build Status" src="https://scan.coverity.com/projects/14645/badge.svg"/> </a>
* Build Dependence: https://github.com/ireader/sdk
 
# libflv
1. Adobe FLV muxer/demuxer
2. MPEG-4 AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord/AV1CodecConfigurationRecord/VPCodecConfigurationRecord/AudioSpecificConfig
3. H.264/H.265 AnnexB to/from MP4 stream
4. AAC ADTS to/from ASC/MUX
5. FLV with H.264/H.264/AV1/VPX(vp8/vp9/vp10)
6. FLV with AAC/mp3/G.711/Opus

# librtmp
1. rtmp-client: RTMP publish/play
2. rtmp-server: RTMP Server live/vod streaming

# libmpeg
1. MPEG-2 PS packer/unpacker
2. MPEG-2 TS packer/unpacker
3. ps/ts with H.264/H.265/AAC/MP3/G.711/Opus

# librtp
1. RFC3550 RTP/RTCP
2. RTP with H.264/H.265/MPEG-2/MPEG-4/VP8/VP9/AV1
2. RTP with G.711/G.726/G.729/MP3/AAC/Opus
3. RTP with MPEG-2 PS/TS

# librtsp
1. RFC 2326 RTSP client
2. RFC 2326 RTSP Server
3. RTSP parser
4. RFC 4566 SDP parser
5. SDP with H.264/H.265/AAC/Opus/G.711 fmtp

# libhls
1. HLS Media: TS segmenter
2. HLS M3U8: generate m3u8 file
3. HLS fmp4 segmenter
4. HLS Master/Playlist m3u8 parser

# libdash
1. ISO/IEC 23009-1 MPEG-DASH static(vod)
2. ISO/IEC 23009-1 MPEG-DASH dynamic(live)
3. DASH MPD v3/v4 parser

# libmov
1. MP4 File reader/writer
2. MP4 faststart(moov box before mdat)
3. Fragment MP4 writer
4. MP4 with H.264/H.265/AV1/VP9
5. MP4 with AAC/Opus/MP3/G.711

# libmkv
1. MKV/WebM file read/write
2. Live MKV/WebM streaming

# libsip
1. sip user-agent (UAC/UAS)
2. sip with ICE

# libhttp(https://github.com/ireader/sdk)
1. HTTP Server(base AIO)
2. HTTP Client
3. HTTP Cookie

### Make
1. make clean && make
2. make RELEASE=1 (make release library, default debug)
3. make PLATFORM=arm-hisiv100nptl-linux (cross compile)

#### [编译说明](compile.cn.md)
