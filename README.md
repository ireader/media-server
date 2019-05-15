* Build status: [![Build Status](https://travis-ci.org/ireader/media-server.svg?branch=master)](https://travis-ci.org/ireader/media-server) <a href="https://scan.coverity.com/projects/ireader-media-server"> <img alt="Coverity Scan Build Status" src="https://scan.coverity.com/projects/14645/badge.svg"/> </a>
* Build Dependence: https://github.com/ireader/sdk
 
# libflv
1. Adobe FLV muxer/demuxer
2. MPEG-4 AVCDecoderConfigurationRecord/AudioSpecificConfig

# librtmp
1. rtmp-client: RTMP publish/play
2. rtmp-server: RTMP Server module

# libmpeg
1. MPEG-2 PS packer/unpacker
2. MPEG-2 TS packer/unpacker
3. H.264/H.265/AAC/MP3

# librtp
1. RFC3550 RTP/RTCP
2. RTP with H.264/H.265/MPEG-2/MPEG-4/VP8/VP9
2. RTP with G.711/G.726/G.729/MP3/AAC/OPUS
3. RTP with MPEG-2 PS/TS

# librtsp
1. RFC 2326 RTSP client
2. RFC 2326 RTSP Server
2. RTSP parser
3. RFC 4566 SDP parser

# libhls
1. HLS Media: TS segmenter
2. HLS M3U8: generate m3u8 file

# libdash
1. ISO/IEC 23009-1 MPEG-DASH static(vod)
2. ISO/IEC 23009-1 MPEG-DASH dynamic(live)

# libmov
1. MP4 File reader/writer
2. MP4 faststart(moov box before mdat)
3. fragment MP4 File writer

# libhttp(https://github.com/ireader/sdk)
1. HTTP Server(base AIO)
2. HTTP Client
3. HTTP Cookie

### Make
1. make clean && make
2. make RELEASE=1 (make release library, default debug)
3. make PLATFORM=arm-hisiv100nptl-linux (cross compile)
