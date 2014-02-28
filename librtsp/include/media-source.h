#ifndef _media_source_h_
#define _media_source_h_

struct IMediaSource
{
	virtual ~IMediaSource(){}

	virtual int Play() = 0;
	virtual int Pause() = 0;
	virtual int Seek(unsigned int pos) = 0;
	virtual int GetDuration(unsigned int& duration) = 0;
};

#endif /* !_media_source_h_ */
