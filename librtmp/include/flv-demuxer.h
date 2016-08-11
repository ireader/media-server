#ifndef _flv_demuxer_h_
#define _flv_demuxer_h_

#if defined(__cplusplus)
extern "C" {
#endif
	enum { FLV_AAC = 1, FLV_AVC };

	typedef void (*flv_demuxer_handler)(void* param, int type, const void* data, unsigned int bytes, unsigned int pts, unsigned int dts);

	void* flv_demuxer_create(flv_demuxer_handler handler, void* param);
	void flv_demuxer_destroy(void* flv);

	int flv_demuxer_input(void* flv, const void* data, unsigned int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_demuxer_h_ */
