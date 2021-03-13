#include "mkv-internal.h"

int mkv_cue_add(struct mkv_t* mkv, int track, int64_t timestamp, uint64_t cluster, uint64_t relative)
{
    void* ptr;
    size_t n;
    struct mkv_cue_position_t* p;

    if (mkv->cue.count >= mkv->cue.capacity)
    {
        n = mkv->cue.count * 3 / 2 + 64;
        ptr = realloc(mkv->cue.positions, sizeof(struct mkv_cue_position_t) * n);
        if (NULL == ptr) return -ENOMEM;

        mkv->cue.capacity = n;
        mkv->cue.positions = ptr;
    }

    p = &mkv->cue.positions[mkv->cue.count++];
    p->track = track;
    p->cluster = cluster;
    p->relative = relative;
    p->timestamp = timestamp;
    return 0;
}

void mkv_write_cues(struct mkv_ioutil_t* io, struct mkv_t* mkv)
{
    size_t i, size;
    uint64_t offset;
    struct mkv_cue_position_t* cue;

    // Segment/Cues
    mkv_buffer_write_master(io, EBML_ID_CUES, 0x0FFFFFFF, 4); // placeholder
    offset = mkv_buffer_tell(io);

    for (i = 0; i < mkv->cue.count; i++)
    {
        cue = &mkv->cue.positions[i];
        size = 3 /*CueTrack*/ + 2 + ebml_uint_length(cue->cluster) + 2 + ebml_uint_length(cue->relative);
        assert(size < 0x7F);

        // Segment/Cues/CuePoint
        mkv_buffer_write_master(io, 0xBB, (2 + ebml_uint_length(cue->timestamp)) /*CueTime*/ + (2 + size) /*CueTrackPositions*/, 0);
        mkv_buffer_write_uint_element(io, 0xB3, cue->timestamp); // CueTime

        // Segment/Cues/CuePoint/CueTrackPositions
        mkv_buffer_write_master(io, 0xB7, size, 0); // only 1
        mkv_buffer_write_uint_element(io, 0xF7, cue->track); // CueTrack
        mkv_buffer_write_uint_element(io, 0xF1, cue->cluster); // CueClusterPosition
        mkv_buffer_write_uint_element(io, 0xF0, cue->relative); // CueRelativePosition
    }

    mkv_write_size(io, offset-4, (uint32_t)(mkv_buffer_tell(io) - offset)); /* update size */
}
