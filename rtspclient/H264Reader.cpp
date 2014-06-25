#include "StdAfx.h"
#include "H264Reader.h"
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define MAX_FRAME_LENGTH (16*1024*1024)

H264Reader::H264Reader()
{
	m_fp = NULL;
	m_buffer = malloc(MAX_FRAME_LENGTH);
	m_length = 0;
	m_framenum = 0;
}

H264Reader::~H264Reader()
{
	Close();
}

int H264Reader::Open(const char* file)
{
	m_fp = fopen(file, "rb");
	if(!m_fp)
		return errno;
	return 0;
}

int H264Reader::Close()
{
	if(m_fp)
	{
		fclose(m_fp);
		m_fp = NULL;
	}

	if(m_buffer)
	{
		free(m_buffer);
		m_buffer = NULL;
	}

	return 0;
}

static unsigned char* search_start_code(unsigned char* stream, int bytes)
{
	unsigned char *p;
	for(p = stream; p+3<stream+bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && 0x00 == p[2] && 0x01 == p[3])
			return p;
	}
	return NULL;
}

int nal_unit(const unsigned char* data, int bytes);

int g_offset = 0;
int H264Reader::Read(OnData callback, void* param)
{
	if(m_length < MAX_FRAME_LENGTH)
	{
		int n = fread((unsigned char*)m_buffer+m_length, 1, MAX_FRAME_LENGTH-m_length, m_fp);
		if(n > 1)
		{
			m_length += n;
			g_offset += n;
		}

		if(0 == m_length)
			return 0;
	}

	unsigned char* startcode = search_start_code((unsigned char*)m_buffer, m_length);
	if(!startcode)
	{
		assert(false);
		return 0;
	}

	int videoSlice = 0;
	m_length -= startcode-(unsigned char*)m_buffer;
	unsigned char* p = startcode;

	do
	{
		p += 4;
		unsigned char* p2 = search_start_code(p, m_length-(p-startcode));
		if(!p2)
			break;

		unsigned int nal_unit_type = 0x1F & p[0];
		if( (nal_unit_type>0 && nal_unit_type <= 5) // data slice
			|| 10==nal_unit_type // end of sequence
			|| 11==nal_unit_type) // end of stream
		{
#if 1
			//int frame_num = nal_unit(p, p2-p);
			//if(frame_num != m_framenum)
			{
				//m_framenum = frame_num;
				callback(param, startcode, p2-startcode);
				memmove(m_buffer, p2, m_length-(p2-startcode));
				m_length -= p2-startcode;
				return 1;
			}
#else
			int frame_num = nal_unit(p, p2-p);
			if(frame_num != m_framenum)
			{
				m_framenum = frame_num;
				callback(param, startcode, p-4-startcode);
				memmove(m_buffer, p-4, m_length-(p-4-startcode));
				m_length -= p-4-startcode;
				return 1;
			}
#endif
		}
		else if(7 == nal_unit_type || 8 == nal_unit_type)
		{
			//nal_unit(p, p2-p);
		}
		else
		{
			if( 10==nal_unit_type // end of sequence
				|| 11==nal_unit_type) // end of stream
			{
				callback(param, startcode, p-startcode);
				memmove(m_buffer, p, m_length-(p-startcode));
				m_length -= p-startcode;
				return 1;
			}
		}

		p = p2;

	} while(startcode);

	// file end ? 
	assert(startcode > 0 && m_length > 0);
	callback(param, startcode, m_length);
	m_length = 0;
	return 1;
}
