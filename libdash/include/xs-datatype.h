#ifndef _xs_datatype_h_
#define _xs_datatype_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @param[in] duration millisecond duration
/// @param[out] data ISO8601 duration: P[n]Y[n]M[n]DT[n]H[n]M[n]S
int xs_duration_write(int64_t duration, char* data, int size);
int xs_duration_read(int64_t* duration, const char* data, int size);

#ifdef __cplusplus
}
#endif
#endif /* !_xs_datatype_h_ */
