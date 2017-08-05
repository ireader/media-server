#include "mov-internal.h"

uint32_t mov_object_to_tag(uint8_t object)
{
	switch (object)
	{
	case MOV_OBJECT_H264:	return MOV_H264;
	case MOV_OBJECT_HEVC:	return MOV_HEVC;
	case MOV_OBJECT_MP4V:	return MOV_MP4V;
	case MOV_OBJECT_AAC:	return MOV_MP4A;
	default: return 0;
	}
}

uint8_t mov_tag_to_object(uint32_t tag)
{
	switch (tag)
	{
	case MOV_H264: return MOV_OBJECT_H264;
	case MOV_HEVC: return MOV_OBJECT_HEVC;
	case MOV_MP4V: return MOV_OBJECT_MP4V;
	case MOV_MP4A: return MOV_OBJECT_AAC;
	default: return 0;
	}
}
