#include "rtp-ext.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://www.arib.or.jp/english/html/overview/doc/STD-T63V12_00/5_Appendix/Rel13/26/26114-d30.pdf
/*
7.4.5 Coordination of Video Orientation
Coordination of Video Orientation consists in signalling of the current orientation of the image captured on the sender
side to the receiver for appropriate rendering and displaying. When CVO is succesfully negotiated it shall be signalled
by the MTSI client. The signalling of the CVO uses RTP Header Extensions as specified in IETF RFC 5285 [95]. The
one-byte form of the header should be used. CVO information for a 2 bit granularity of Rotation (corresponding to
urn:3gpp:video-orientation) is carried as a byte formatted as follows:
    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID   | len=0 |0 0 0 0 C F R R|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	Bit#		7 6 5 4 3 2 1  0(LSB)
	Definition  0 0 0 0 C F R1 R0

With the following definitions:
C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in
    receiver to e.g. display the received video differently depending on the source camera.
	0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
	1: Back-facing camera, facing away from the user.
F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
	0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
	1: Horizontal flip operation
R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to
	compensate that rotation. E.g. a 90бу Counter Clockwise rotation should be compensated by the receiver with a 90бу
	Clockwise rotation prior to displaying.

Table 7.2: Rotation signalling for 2 bit granularity
R1 R0 Rotation of the video as sent on the link		Rotation on the receiver before display
0  0	0бу rotation									None
0  1	90бу Counter Clockwise (CCW) rotation or		90бу CW rotation
		270бу Clockwise (CW) rotation
1  0	180бу CCW rotation or 180бу CW rotation		180бу CW rotation
1  1	270бу CCW rotation or 90бу CW rotation		90бу CCW rotation

CVO information for a higher granularity of Rotation (corresponding to urn:3GPP:video-orientation:6) is carried as a
byte formatted as follows:
	Bit#       7   6  5  4 3 2  1 0(LSB)
	Definition R5 R4 R3 R2 C F R1 R0

where C and F are as defined above and the bits R5,R4,R3,R2,R1,R0 represent the Rotation, which indicates the
rotation of the video as transmitted on the link. Table 7.3 describes the rotation to be applied by the receiver based on
the rotation bits.

Table 7.3: Rotation signalling for 6 bit granularity
R1	R0	R5	R4	R3	R2	Rotation of the video as		Rotation on the receiver 
						sent on the link				before display
0	0	0	0	0	0	0бу rotation						None
0	0	0	0	0	1	(360/64)бу Counter Clockwise		(360/64)бу CW rotation
						(CCW) rotation
0	0	0	0	1	0	(2*360/64)бу CCW rotation		(2*360/64)бу CW rotation
.	.	.	.	.	.	.								.
.	.	.	.	.	.	.								.
.	.	.	.	.	.	.								.
1	1	1	1	1	0	(62*360/64)бу CCW rotation		(2*360/64)бу CCW rotation
1	1	1	1	1	1	(63*360/64)бу CCW rotation		(360/64)бу CCW rotation
*/

int rtp_ext_video_orientation_parse(const uint8_t* data, int bytes, struct rtp_ext_video_orientation_t *ext)
{
	assert(1 == bytes);
	if (bytes < 1)
		return -1;

	ext->camera = (data[0] >> 3) & 0x01;
	ext->flip = (data[0] >> 2) & 0x01;
	ext->rotaion = (data[0] & 0x03) * 90;
	return 0;
}

int rtp_ext_video_orientation_write(uint8_t* data, int bytes, const struct rtp_ext_video_orientation_t* ext)
{
	if (bytes < 1)
		return -1;
	
	data[0] = ext->camera ? 0x04 : 0;
	data[0] |= ext->flip ? 0x03 : 0;
	data[0] |= (ext->rotaion / 90) & 0x03;
	return 1;
}
