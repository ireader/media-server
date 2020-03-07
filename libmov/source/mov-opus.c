#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// http://www.opus-codec.org/docs/opus_in_isobmff.html
// 4.3.2 Opus Specific Box
/*
class ChannelMappingTable (unsigned int(8) OutputChannelCount){
    unsigned int(8) StreamCount;
    unsigned int(8) CoupledCount;
    unsigned int(8 * OutputChannelCount) ChannelMapping;
}
 
aligned(8) class OpusSpecificBox extends Box('dOps'){
    unsigned int(8) Version;
    unsigned int(8) OutputChannelCount;
    unsigned int(16) PreSkip;
    unsigned int(32) InputSampleRate;
    signed int(16) OutputGain;
    unsigned int(8) ChannelMappingFamily;
    if (ChannelMappingFamily != 0) {
        ChannelMappingTable(OutputChannelCount);
    }
}
*/

int mov_read_dops(struct mov_t* mov, const struct mov_box_t* box)
{
    struct mov_track_t* track = mov->track;
    struct mov_sample_entry_t* entry = track->stsd.current;
    if(box->size >= 10)
    {
        if (entry->extra_data_size < box->size + 8)
        {
            void* p = realloc(entry->extra_data, (size_t)box->size + 8);
            if (NULL == p) return ENOMEM;
            entry->extra_data = p;
        }
        
        memcpy(entry->extra_data, "OpusHead", 8);
        entry->extra_data[8] = 1; // OpusHead version
        mov_buffer_r8(&mov->io); // version 0
        entry->extra_data[9] = mov_buffer_r8(&mov->io); // channel
        entry->extra_data[11] = mov_buffer_r8(&mov->io); // PreSkip (MSB -> LSB)
        entry->extra_data[10] = mov_buffer_r8(&mov->io);
        entry->extra_data[15] = mov_buffer_r8(&mov->io); // InputSampleRate (LSB -> MSB)
        entry->extra_data[14] = mov_buffer_r8(&mov->io);
        entry->extra_data[13] = mov_buffer_r8(&mov->io);
        entry->extra_data[12] = mov_buffer_r8(&mov->io);
        entry->extra_data[17] = mov_buffer_r8(&mov->io); // OutputGain (LSB -> MSB)
        entry->extra_data[16] = mov_buffer_r8(&mov->io);
        mov_buffer_read(&mov->io, entry->extra_data + 18, (size_t)box->size - 10);
        entry->extra_data_size = (int)box->size + 8;
    }
    return mov_buffer_error(&mov->io);
}

size_t mov_write_dops(const struct mov_t* mov)
{
    const struct mov_track_t* track = mov->track;
    const struct mov_sample_entry_t* entry = track->stsd.current;
    if (entry->extra_data_size < 18)
        return 0;
    
    assert(0 == memcmp(entry->extra_data, "OpusHead", 8));
    mov_buffer_w32(&mov->io, entry->extra_data_size); /* size */
    mov_buffer_write(&mov->io, "dOps", 4);
    mov_buffer_w8(&mov->io, 0); // The Version field shall be set to 0.
    mov_buffer_w8(&mov->io, entry->extra_data[9]); // channel count
    mov_buffer_w16(&mov->io, (entry->extra_data[11]<<8) | entry->extra_data[10]); // PreSkip (LSB -> MSB)
    mov_buffer_w32(&mov->io, (entry->extra_data[15]<<8) | (entry->extra_data[14]<<8) | (entry->extra_data[13]<<8) | entry->extra_data[12]); // InputSampleRate (LSB -> MSB)
    mov_buffer_w32(&mov->io, (entry->extra_data[17]<<8) | entry->extra_data[16]); // OutputGain (LSB -> MSB)
    mov_buffer_write(&mov->io, entry->extra_data + 18, entry->extra_data_size - 18);
    return entry->extra_data_size;
}
