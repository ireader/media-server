#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// https://www.webmproject.org/vp9/mp4/

int mov_read_smdm(struct mov_t* mov, const struct mov_box_t* box)
{
    (void)box;
    mov_buffer_r8(&mov->io); // version
    mov_buffer_r24(&mov->io); // flags
    
    mov_buffer_r16(&mov->io); // primaryRChromaticity_x, 0.16 fixed-point Red X chromaticity coordinate as defined by CIE 1931
    mov_buffer_r16(&mov->io); // primaryRChromaticity_y
    mov_buffer_r16(&mov->io); // primaryGChromaticity_x
    mov_buffer_r16(&mov->io); // primaryGChromaticity_y
    mov_buffer_r16(&mov->io); // primaryBChromaticity_x
    mov_buffer_r16(&mov->io); // primaryBChromaticity_y
    mov_buffer_r16(&mov->io); // whitePointChromaticity_x
    mov_buffer_r16(&mov->io); // whitePointChromaticity_y
    mov_buffer_r32(&mov->io); // luminanceMax, 24.8 fixed point Maximum luminance, represented in candelas per square meter (cd/m²)
    mov_buffer_r32(&mov->io); // luminanceMin
    
    return mov_buffer_error(&mov->io);
}

int mov_read_coll(struct mov_t* mov, const struct mov_box_t* box)
{
    (void)box;
    mov_buffer_r8(&mov->io); // version
    mov_buffer_r24(&mov->io); // flags
    
    mov_buffer_r16(&mov->io); // maxCLL, Maximum Content Light Level as specified in CEA-861.3, Appendix A.
    mov_buffer_r16(&mov->io); // maxFALL, Maximum Frame-Average Light Level as specified in CEA-861.3, Appendix A.
    return mov_buffer_error(&mov->io);
}
