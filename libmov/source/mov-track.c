#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FREE(p) do { if(p) free(p); } while(0)

struct mov_track_t* mov_add_track(struct mov_t* mov)
{
    void* ptr = NULL;
    struct mov_track_t* track;

    ptr = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
    if (NULL == ptr) return NULL;

    mov->tracks = ptr;
    track = &mov->tracks[mov->track_count];
    memset(track, 0, sizeof(struct mov_track_t));
    track->start_dts = INT64_MIN;

    track->stsd.entries = calloc(1, sizeof(struct mov_sample_entry_t));
    if (NULL == track->stsd.entries)
        return NULL;
    track->stsd.current = track->stsd.entries;

    return track;
}

void mov_free_track(struct mov_track_t* track)
{
    size_t i;
    for (i = 0; i < track->sample_count; i++)
    {
        if (track->samples[i].data)
            free(track->samples[i].data);
    }
	
	for (i = 0; i < track->stsd.entry_count; i++)
	{
		if (track->stsd.entries[i].extra_data)
			free(track->stsd.entries[i].extra_data);
	}

    FREE(track->elst);
    FREE(track->frags);
    FREE(track->samples);
//    FREE(track->extra_data);
    FREE(track->stsd.entries);
    FREE(track->stbl.stco);
    FREE(track->stbl.stsc);
    FREE(track->stbl.stss);
    FREE(track->stbl.stts);
    FREE(track->stbl.ctts);
}

struct mov_track_t* mov_find_track(const struct mov_t* mov, uint32_t track)
{
    size_t i;
    for (i = 0; i < mov->track_count; i++)
    {
        if (mov->tracks[i].tkhd.track_ID == track)
            return mov->tracks + i;
    }
    return NULL;
}

struct mov_track_t* mov_fetch_track(struct mov_t* mov, uint32_t track)
{
    struct mov_track_t* t;
    t = mov_find_track(mov, track);
    if (NULL == t)
    {
        t = mov_add_track(mov);
        if (NULL != t)
        {
            ++mov->track_count;
            t->tkhd.track_ID = track;
        }
    }
    return t;
}

int mov_add_audio(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
    struct mov_sample_entry_t* audio;

    audio = &track->stsd.entries[0];
    audio->data_reference_index = 1;
    audio->object_type_indication = object;
    audio->stream_type = MP4_STREAM_AUDIO;
    audio->u.audio.channelcount = (uint16_t)channel_count;
    audio->u.audio.samplesize = (uint16_t)bits_per_sample;
    audio->u.audio.samplerate = (sample_rate > 56635 ? 0 : sample_rate) << 16;

    assert(0 != mov_object_to_tag(object));
    track->tag = mov_object_to_tag(object);
    track->handler_type = MOV_AUDIO;
    track->handler_descr = "SoundHandler";
    track->stsd.entry_count = 1;
    track->offset = 0;

    track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
    track->tkhd.track_ID = mvhd->next_track_ID;
    track->tkhd.creation_time = mvhd->creation_time;
    track->tkhd.modification_time = mvhd->modification_time;
    track->tkhd.width = 0;
    track->tkhd.height = 0;
    track->tkhd.volume = 0x0100;
    track->tkhd.duration = 0; // placeholder

    track->mdhd.creation_time = track->tkhd.creation_time;
    track->mdhd.modification_time = track->tkhd.modification_time;
    track->mdhd.timescale = timescale; //sample_rate
    track->mdhd.language = 0x55c4;
    track->mdhd.duration = 0; // placeholder

    audio->extra_data = malloc(extra_data_size + 1);
    if (NULL == audio->extra_data)
        return -ENOMEM;
    memcpy(audio->extra_data, extra_data, extra_data_size);
	audio->extra_data_size = extra_data_size;

    return 0;
}

int mov_add_video(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
    struct mov_sample_entry_t* video;

    video = &track->stsd.entries[0];
    video->data_reference_index = 1;
    video->object_type_indication = object;
    video->stream_type = MP4_STREAM_VISUAL;
    video->u.visual.width = (uint16_t)width;
    video->u.visual.height = (uint16_t)height;
    video->u.visual.depth = 0x0018;
    video->u.visual.frame_count = 1;
    video->u.visual.horizresolution = 0x00480000;
    video->u.visual.vertresolution = 0x00480000;

    assert(0 != mov_object_to_tag(object));
    track->tag = mov_object_to_tag(object);
    track->handler_type = MOV_VIDEO;
    track->handler_descr = "VideoHandler";
    track->stsd.entry_count = 1;
    track->offset = 0;

    track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
    track->tkhd.track_ID = mvhd->next_track_ID;
    track->tkhd.creation_time = mvhd->creation_time;
    track->tkhd.modification_time = mvhd->modification_time;
    track->tkhd.width = width << 16;
    track->tkhd.height = height << 16;
    track->tkhd.volume = 0;
    track->tkhd.duration = 0; // placeholder

    track->mdhd.creation_time = track->tkhd.creation_time;
    track->mdhd.modification_time = track->tkhd.modification_time;
    track->mdhd.timescale = timescale; //mov->mvhd.timescale
    track->mdhd.language = 0x55c4;
    track->mdhd.duration = 0; // placeholder

	video->extra_data = malloc(extra_data_size + 1);
    if (NULL == video->extra_data)
        return -ENOMEM;
    memcpy(video->extra_data, extra_data, extra_data_size);
	video->extra_data_size = extra_data_size;

    return 0;
}

int mov_add_subtitle(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, const void* extra_data, size_t extra_data_size)
{
    struct mov_sample_entry_t* subtitle;

    subtitle = &track->stsd.entries[0];
    subtitle->data_reference_index = 1;
    subtitle->object_type_indication = object;
    subtitle->stream_type = MP4_STREAM_VISUAL; // Visually composed tracks including video and text are layered using the ¡®layer¡¯ value.

    assert(0 != mov_object_to_tag(object));
    track->tag = mov_object_to_tag(object);
    track->handler_type = MOV_SUBT;
    track->handler_descr = "SubtitleHandler";
    track->stsd.entry_count = 1;
    track->offset = 0;

    track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
    track->tkhd.track_ID = mvhd->next_track_ID;
    track->tkhd.creation_time = mvhd->creation_time;
    track->tkhd.modification_time = mvhd->modification_time;
    track->tkhd.width = 0;
    track->tkhd.height = 0;
    track->tkhd.volume = 0;
    track->tkhd.duration = 0; // placeholder

    track->mdhd.creation_time = track->tkhd.creation_time;
    track->mdhd.modification_time = track->tkhd.modification_time;
    track->mdhd.timescale = timescale;
    track->mdhd.language = 0x55c4;
    track->mdhd.duration = 0; // placeholder

	subtitle->extra_data = malloc(extra_data_size + 1);
    if (NULL == subtitle->extra_data)
        return -ENOMEM;
    memcpy(subtitle->extra_data, extra_data, extra_data_size);
	subtitle->extra_data_size = extra_data_size;

    return 0;
}

// ISO/IEC 14496-12:2012(E) 6.2.3 Box Order (p23)
// It is recommended that the boxes within the Sample Table Box be in the following order: 
// Sample Description, Time to Sample, Sample to Chunk, Sample Size, Chunk Offset.
size_t mov_write_stbl(const struct mov_t* mov)
{
    size_t size;
    uint32_t count;
    uint64_t offset;
    struct mov_track_t* track;
    track = (struct mov_track_t*)mov->track;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "stbl", 4);

    size += mov_write_stsd(mov);

    count = mov_build_stts(track);
    size += mov_write_stts(mov, count);
    if (track->tkhd.width > 0 && track->tkhd.height > 0)
        size += mov_write_stss(mov); // video only
    count = mov_build_ctts(track);
    if (track->sample_count > 0 && (count > 1 || track->samples[0].samples_per_chunk != 0))
        size += mov_write_ctts(mov, count);

    count = mov_build_stco(track);
    size += mov_write_stsc(mov);
    size += mov_write_stsz(mov);
    size += mov_write_stco(mov, count);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

size_t mov_write_minf(const struct mov_t* mov)
{
    size_t size;
    uint64_t offset;
    const struct mov_track_t* track = mov->track;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "minf", 4);

    if (MOV_VIDEO == track->handler_type)
    {
        size += mov_write_vmhd(mov);
    }
    else if (MOV_AUDIO == track->handler_type)
    {
        size += mov_write_smhd(mov);
    }
    else if (MOV_SUBT == track->handler_type)
    {
        size += mov_write_nmhd(mov);
    }
    else
    {
        assert(0);
    }

    size += mov_write_dinf(mov);
    size += mov_write_stbl(mov);
    mov_write_size(mov, offset, size); /* update size */
    return size;
}

size_t mov_write_mdia(const struct mov_t* mov)
{
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "mdia", 4);

    size += mov_write_mdhd(mov);
    size += mov_write_hdlr(mov);
    size += mov_write_minf(mov);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

size_t mov_write_trak(const struct mov_t* mov)
{
    size_t size;
    uint64_t offset;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "trak", 4);

    size += mov_write_tkhd(mov);
    //size += mov_write_tref(mov);
    size += mov_write_edts(mov);
    size += mov_write_mdia(mov);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}

size_t mov_write_edts(const struct mov_t* mov)
{
    size_t size;
    uint64_t offset;

    if (mov->track->sample_count < 1)
        return 0;

    size = 8 /* Box */;
    offset = mov_buffer_tell(&mov->io);
    mov_buffer_w32(&mov->io, 0); /* size */
    mov_buffer_write(&mov->io, "edts", 4);

    size += mov_write_elst(mov);

    mov_write_size(mov, offset, size); /* update size */
    return size;
}
