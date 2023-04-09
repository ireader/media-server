* Build status: <a href="https://scan.coverity.com/projects/ireader-media-server"> <img alt="Coverity Scan Build Status" src="https://scan.coverity.com/projects/14645/badge.svg"/> </a>
* Build Dependence: https://github.com/ireader/sdk
 
# libflv
1. FLV video codec: H.264/H.265/H.266/AV1/VP8/VP9/VP10
2. FLV audio codec: AAC/MP3/G.711/Opus
3. FLV file read/write
4. H.264/H.265/H.266/AV1 bitstream filter: annex-b <-> mp4 stream
5. AAC bitstream filter: ADTS <-> ASC

# librtmp
1. rtmp-client: RTMP publish/play
2. rtmp-server: RTMP Server live/vod streaming

# libmpeg
1. ITU-T H.222.0 PS/TS read/write
2. ps/ts codec: H.264/H.265/H.266/AAC/MP3/G.711/Opus

# librtp
1. RFC3550 RTP/RTCP
2. RTP with H.264/H.265/H.266/MPEG-2/MPEG-4/VP8/VP9/AV1
3. RTP with G.711/G.726/G.729/MP3/AAC/Opus
4. RTP with MPEG-2 PS/TS
5. RTP Header Extension
6. RTCP PSFB/RTPFB/XR

# librtsp
1. RFC2326 RTSP
2. RFC4566 SDP
3. SDP fmtp: H.264/H.265/H.266/AAC/Opus/G.711 

# libhls
1. HLS M3U8: generate m3u8 file
2. HLS Media: TS segmenter
3. HLS fmp4 segmenter
4. HLS Master/Playlist m3u8 parser

# libdash
1. ISO/IEC 23009-1 MPEG-DASH static(vod)
2. ISO/IEC 23009-1 MPEG-DASH dynamic(live)
3. DASH MPD v3/v4 parser

# libmov
1. ISO/IEC 14496-12 MP4 File reader/writer
2. MP4 faststart(moov box before mdat)
3. fMP4(Fragment MP4) writer
4. MP4 with H.264/H.265/H.266/AV1/VP8/VP9/JPEG/PNG
5. MP4 with AAC/Opus/MP3/G.711

# libmkv
1. MKV/WebM file read/write
2. MKV/WebM live streaming

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
