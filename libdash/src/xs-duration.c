#include "xs-datatype.h"
#include <stdio.h>
#include <time.h>

// https://en.wikipedia.org/wiki/ISO_8601
// Durations: 
// 1. P[n]Y[n]M[n]DT[n]H[n]M[n]S
// 2. P[n]W
// 3. P<date>T<time>
// 4. PYYYYMMDDThhmmss
// 5. P[YYYY]-[MM]-[DD]T[hh]:[mm]:[ss]
// For example, "P3Y6M4DT12H30M5S" represents a duration of "three years, six months, four days, twelve hours, thirty minutes, and five seconds".
// "P23DT23H" and "P4Y" "P0.5Y" == "P0,5Y"
// "PT0S" or "P0D"
// "P0003-06-04T12:30:05"

/// @param[in] duration millisecond duration
/// @param[out] data ISO8601 duration: P[n]Y[n]M[n]DT[n]H[n]M[n]S
int xs_duration_write(int64_t duration, char* data, int size)
{
    int n = 1;
    data[0] = 'P';
    if (duration > 24 * 3600 * 1000)
    {
        n += snprintf(data + n, size - n, "%dD", (int)(duration / (24 * 3600 * 1000)));
        duration %= 24 * 3600 * 1000;
    }

    data[n++] = 'T';
    if (duration > 3600 * 1000)
    {
        n += snprintf(data + n, size - n, "%dH", (int)(duration / (3600 * 1000)));
        duration %= 3600 * 1000;

        n += snprintf(data + n, size - n, "%dM", (int)(duration / (60 * 1000)));
        duration %= 60 * 1000;
    }

    n += snprintf(data + n, size - n, "%dS", (int)((duration + 999) / 1000));
    duration %= 1000;

    return n;
}

int xs_duration_read(int64_t* duration, const char* data, int size)
{
    int n;
    int flags;
    unsigned int v1[6];
    float v2[6];
    const char* ptr;
    const char* end;

    if (!data || size < 2)
        return -1;

    ptr = data;
    end = data + size;

    n = flags = 0;
    v1[0] = v1[1] = v1[2] = v1[3] = v1[4] = v1[5] = 0;
    v2[0] = v2[1] = v2[2] = v2[3] = v2[4] = v2[5] = 0.0;

    if ('-' == ptr[0])
    {
        flags = 1;
        ptr++;
    }

    if ('P' != ptr[0])
        return -1;
    ptr++;
    
    // P20200529T164700
    if (6 == sscanf(ptr, "%4u%2u%2uT%2u%2u%2u", &v1[0], &v1[1], &v1[2], &v1[3], &v1[4], &v1[5]))
    {
        *duration = (v1[5] % 60) * 1000LL;
        *duration += (v1[4] % 60) * 60LL * 1000;
        *duration += (v1[3] % 24) * 60LL * 60 * 1000;
        *duration += v1[2] * 24LL * 60 * 60 * 1000;
        //*duration += v1[1] * 30LL * 24 * 60 * 60 * 1000;
        //*duration += v1[0] * 365LL * 30 * 24 * 60 * 60 * 1000;
    }
    // P2020-05-29T16:47:00
    else if (6 == sscanf(ptr, "%u-%u-%uT%u:%u:%u", &v1[0], &v1[1], &v1[2], &v1[3], &v1[4], &v1[5]))
    {
        *duration = (v1[5] % 60) * 1000LL;
        *duration += (v1[4] % 60) * 60LL * 1000;
        *duration += (v1[3] % 24) * 60LL * 60 * 1000;
        *duration += v1[2] * 24LL * 60LL * 60 * 1000;
        //*duration += v1[1] * 30LL * 24 * 60 * 60 * 1000;
        //*duration += v1[0] * 365LL * 30 * 24 * 60 * 60 * 1000;
    }
    // P[n]W
    else if (1 == sscanf(ptr, "%fW%n", &v2[0], &n))
    {
        *duration = (int64_t)(v2[0] * 7.0 * 24 * 60 * 60 * 1000);
    }
    else
    {
        if (1 == sscanf(ptr, "%fY%n", &v2[0], &n))
            ptr += n;
        if (1 == sscanf(ptr, "%fM%n", &v2[1], &n))
            ptr += n;
        if (1 == sscanf(ptr, "%fD%n", &v2[2], &n))
            ptr += n;

        if ('T' == ptr[0])
        {
            ptr++;
            if (1 == sscanf(ptr, "%fH%n", &v2[3], &n))
                ptr += n;
            if (1 == sscanf(ptr, "%fM%n", &v2[4], &n))
                ptr += n;
            if (1 == sscanf(ptr, "%fS%n", &v2[5], &n))
                ptr += n;
        }

        *duration = (int64_t)(v2[5] * 1000.0);
        *duration += (int64_t)(v2[4] * 60.0 * 1000);
        *duration += (int64_t)(v2[3] * 60.0 * 60 * 1000);
        *duration += (int64_t)(v2[2] * 24.0 * 60 * 60 * 1000);
        //*duration += (int64_t)(v2[1] * 30.0 * 24 * 60 * 60 * 1000);
        //*duration += (int64_t)(v2[0] * 365.0 * 30 * 24 * 60 * 60 * 1000);
    }

    return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void xs_datatype_test(void)
{
    int64_t duation;
    xs_duration_read(&duation, "P3Y6M4DT12H30M5S", 16);
    xs_duration_read(&duation, "PT12H30M5S", 10);
    xs_duration_read(&duation, "P0.5Y", 5);
    xs_duration_read(&duation, "P1W", 3);
    xs_duration_read(&duation, "PT1.2S", 6);
    xs_duration_read(&duation, "P00030604T164700", 16);
    xs_duration_read(&duation, "P0003-06-04T12:30:05", 20);
    xs_duration_read(&duation, "-PT30M5S", 8);
}
#endif
