#librtmp
1. rtmp-client send rtmp packet to server
2. rtmp-reader receive rtmp packet from server
3. flv-demuxer Adobe FLV demuxer
4. other: MPEG-4 AVCDecoderConfigurationRecord/AudioSpecificConfig

#libmpeg
1. MPEG-2 PS packer/unpacker
2. MPEG-2 TS packer/unpacker

#librtp
1. RFC3550 RTP/RTCP packer/unpacker
2. RTP with H.264
3. RTP with MPEG-2 PS

#librtsp
1. RFC 2326 RTSP packer/unpacker
2. RTSP text parser
3. RFC 4566 SDP parser
4. RFC 822 datetime parser

#libhls
1. HLS Vod
2. HLS Live(not ready)
3. HLS Server(not ready)

#libhttp(https://github.com/ireader/sdk)
1. HTTP Server(base AIO)
2. HTTP Client
3. HTTP Cookie
