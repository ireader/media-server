#ifndef _h264_file_reader_h_
#define _h264_file_reader_h_

#include <stdio.h>
#include "mmptr.h"

class H264FileReader
{
public:
	H264FileReader(const char* file);
	~H264FileReader();

	bool IsOpened() const;

public:
	const mmptr& GetSPSandPPS() const;
	int GetDuration(unsigned int& duration);
	int GetNextFrame();
	int Seek(unsigned int pos);

private:
	int Init();
	int GetNextNALU(unsigned char*& data, int& bytes);

private:
	FILE* m_fp;
	mmptr m_ptr;
	mmptr m_spspps;
};

#endif /* !_h264_file_reader_h_ */
