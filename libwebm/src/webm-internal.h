#ifndef _webm_internal_h_
#define _webm_internal_h_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "webm-buffer.h"

struct webm_track_t
{
	int id;
};

struct webm_t
{
	struct webm_track_t* tracks;
	int track_count;
};

#endif /* !_webm_internal_h_ */
