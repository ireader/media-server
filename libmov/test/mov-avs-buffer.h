#ifndef _mov_file_buffer_h_
#define _mov_file_buffer_h_

#if defined(__AVS__)

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const struct mov_buffer_t* mov_avs_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* __AVS__ */
#endif /* !_mov_file_buffer_h_ */
