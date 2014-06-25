#ifndef _SVACFileReader_h_
#define _SVACFileReader_h_

#include <stdio.h>

class H264Reader
{
public:
	H264Reader();
	~H264Reader();

	int Open(const char* file);
	int Close();

	typedef void (*OnData)(void* param, const void* data, int bytes);
	int Read(OnData callback, void* param);

private:
	FILE* m_fp;
	void* m_buffer;
	int m_length;
	int m_framenum;
};

#endif /* !_SVACFileReader_h_ */
