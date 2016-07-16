#include "librtmp/rtmp.h"
#include <string.h>
#include <assert.h>

static char* amf_encode_string(char* out, char* outend, const char* s)
{
	AVal v;
	v.av_len = strlen(s);
	v.av_val = (char*)s;

	return AMF_EncodeString(out, outend, &v);
}

static char* amf_encode_name_value_string(char* out, char* outend, const char* name, const char* value)
{
	AVal n, v;
	n.av_len = strlen(name);
	n.av_val = (char*)name;
	v.av_len = strlen(value);
	v.av_val = (char*)value;

	return AMF_EncodeNamedString(out, outend, &n, &v);
}

static char* amf_encode_name_value_double(char* out, char* outend, const char* name, double value)
{
	AVal n;
	n.av_len = strlen(name);
	n.av_val = (char*)name;

	return AMF_EncodeNamedNumber(out, outend, &n, value);
}

char* rtmp_metadata_create(char* out, size_t len, int width, int height, int audio)
{
	char* outend = out + len;
	
	out = amf_encode_string(out, outend, "@setDataFrame");
	out = amf_encode_string(out, outend, "onMetaData");

	// AMF_Encode(AMFObject *obj, char *pBuffer, char *pBufEnd)
	*out++ = AMF_OBJECT;

//	out = amf_encode_name_value_string(out, outend, "author", "Suicam Technology (Beijing) Co., LTD.");
	out = amf_encode_name_value_string(out, outend, "copyright", "Suicam Technology (Beijing) Co., LTD.");
//	out = amf_encode_name_value_string(out, outend, "description", "Suicam live");
//	out = amf_encode_name_value_string(out, outend, "keywords", "Suicam");
//	out = amf_encode_name_value_string(out, outend, "rating", "");
//	out = amf_encode_name_value_string(out, outend, "presetname", "");
	if (width > 0 && height > 0)
	{
		//out = amf_encode_name_value_string(out, outend, "videocodecid", "AVC1"); // FLV VIDEODATA AVC/H.264
		out = amf_encode_name_value_double(out, outend, "videocodecid", (double)7); // FLV VIDEODATA AVC/H.264
		out = amf_encode_name_value_double(out, outend, "width", (double)width);
		out = amf_encode_name_value_double(out, outend, "height", (double)height);
		out = amf_encode_name_value_double(out, outend, "framerate", 25.0);
		//out = amf_encode_name_value_double(out, outend, "videodatarate", 2000000.0);
		//out = amf_encode_name_value_double(out, outend, "duration", 0.0);
		//out = amf_encode_name_value_double(out, outend, "avclevel", );
		//out = amf_encode_name_value_double(out, outend, "avcprofile", );
		//out = amf_encode_name_value_double(out, outend, "videokeyframe_frequency", 2.0);
	}

	if (audio)
	{
//		out = amf_encode_name_value_string(out, outend, "audiocodecid", "mp4a"); // FLV AUDIODATA AAC
		out = amf_encode_name_value_double(out, outend, "audiocodecid", 10.0); // FLV AUDIODATA AAC
		out = amf_encode_name_value_double(out, outend, "audiodatarate", 44100.0);
		out = amf_encode_name_value_double(out, outend, "audiosamplesize", 16.0);
		out = amf_encode_name_value_double(out, outend, "audiochannels", 2.0);
//		out = amf_encode_name_value_double(out, outend, "audiosamplerate", );
	}

	out = AMF_EncodeInt24(out, outend, AMF_OBJECT_END);
	*out = 0;
	return out;
}
