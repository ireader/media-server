#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.5 Chunk Offset Box (p58)
/*
aligned(8) class ChunkOffsetBox extends FullBox('stco', version = 0, 0) { 
	unsigned int(32) entry_count; 
	for (i=1; i <= entry_count; i++) { 
		unsigned int(32) chunk_offset; 
	} 
}

aligned(8) class ChunkLargeOffsetBox extends FullBox('co64', version = 0, 0) { 
	unsigned int(32) entry_count; 
	for (i=1; i <= entry_count; i++) { 
		unsigned int(64) chunk_offset; 
	} 
}
*/

int mov_read_stco(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == stbl->stco_count && NULL == stbl->stco);
	if (stbl->stco_count < entry_count)
	{
		void* p = realloc(stbl->stco, sizeof(stbl->stco[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stco = p;
	}
	stbl->stco_count = entry_count;

	if (MOV_TAG('s', 't', 'c', 'o') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			stbl->stco[i] = mov_buffer_r32(&mov->io); // chunk_offset
	}
	else if (MOV_TAG('c', 'o', '6', '4') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			stbl->stco[i] = mov_buffer_r64(&mov->io); // chunk_offset
	}
	else
	{
		i = 0;
		assert(0);
	}

	stbl->stco_count = i;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_stco(const struct mov_t* mov, uint32_t count)
{
	int co64;
	uint32_t size, i;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	sample = track->sample_count > 0 ? &track->samples[track->sample_count - 1] : NULL;
	co64 = (sample && sample->offset + track->offset > UINT32_MAX) ? 1 : 0;
	size = 12/* full box */ + 4/* entry count */ + count * (co64 ? 8 : 4);

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, co64 ? "co64" : "stco", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = track->samples + i;
		if(0 == sample->first_chunk)
			continue;

		if(0 == co64)
			mov_buffer_w32(&mov->io, (uint32_t)(sample->offset + track->offset));
		else
			mov_buffer_w64(&mov->io, sample->offset + track->offset);
	}

	return size;
}

size_t mov_stco_size(const struct mov_track_t* track, uint64_t offset)
{
	size_t i, j;
	uint64_t co64;
	const struct mov_sample_t* sample;

	if (track->sample_count < 1)
		return 0;

	sample = &track->samples[track->sample_count - 1];
	co64 = sample->offset + track->offset;
	if (co64 > UINT32_MAX || co64 + offset <= UINT32_MAX)
		return 0;

	for (i = 0, j = 0; i < track->sample_count; i++)
	{
		sample = track->samples + i;
		if (0 != sample->first_chunk)
			j++;
	}

	return j * 4;
}

uint32_t mov_build_stco(struct mov_track_t* track)
{
    size_t i;
    size_t bytes = 0;
    uint32_t count = 0;
    struct mov_sample_t* sample = NULL;

    assert(track->stsd.entry_count > 0);
    for (i = 0; i < track->sample_count; i++)
    {
        if (NULL != sample
            && sample->offset + bytes == track->samples[i].offset
            && sample->sample_description_index == track->samples[i].sample_description_index)
        {
            track->samples[i].first_chunk = 0; // mark invalid value
            bytes += track->samples[i].bytes;
            ++sample->samples_per_chunk;
        }
        else
        {
            sample = &track->samples[i];
            sample->first_chunk = ++count; // chunk start from 1
            sample->samples_per_chunk = 1;
            bytes = sample->bytes;
        }
    }

    return count;
}

void mov_apply_stco(struct mov_track_t* track)
{
    uint32_t i, j, k;
    uint64_t n, chunk_offset;
    struct mov_stbl_t* stbl = &track->stbl;

    // sample offset
    assert(stbl->stsc_count > 0 && stbl->stco_count > 0);
    stbl->stsc[stbl->stsc_count].first_chunk = stbl->stco_count + 1; // fill stco count
    for (i = 0, n = 0; i < stbl->stsc_count; i++)
    {
        assert(stbl->stsc[i].first_chunk <= stbl->stco_count);
        for (j = stbl->stsc[i].first_chunk; j < stbl->stsc[i + 1].first_chunk; j++)
        {
            chunk_offset = stbl->stco[j - 1]; // chunk start from 1
            for (k = 0; k < stbl->stsc[i].samples_per_chunk; k++, n++)
            {
                track->samples[n].sample_description_index = stbl->stsc[i].sample_description_index;
                track->samples[n].offset = chunk_offset;
                track->samples[n].data = NULL;
                chunk_offset += track->samples[n].bytes;
                assert(track->samples[n].bytes > 0);
                assert(0 == n || track->samples[n - 1].offset + track->samples[n - 1].bytes <= track->samples[n].offset);
            }
        }
    }

    assert(n == track->sample_count);
}
