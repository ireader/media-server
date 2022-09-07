#include "mkv-writer.h"
#include "mkv-internal.h"
#include <time.h>

#define MKV_APP "ireader/media-server"
#define MKV_SEEK_HEAD_RESERVED 80

struct mkv_writer_t
{
    int options;
    struct mkv_t mkv;
    struct mkv_ioutil_t io;
    struct mkv_cluster_t cluster;

    // config
    int use_dts;
    size_t cluster_limit_size;
    int64_t cluster_limit_duration;

    int has_video;
    int64_t segment_ts; // segment offset timecode

    uint64_t info_duration_offset;
    uint64_t seekhead_offset;
    uint64_t segment_offset;
};

static void mkv_write_void(struct mkv_ioutil_t* io, uint32_t size)
{
    assert(size >= 2);
    if (size < 10)
    {
        // 1-byte length
        mkv_buffer_write_master(io, EMLB_ID_VOID, size - 2, 0);
        mkv_buffer_skip(io, size - 2);
    }
    else
    {
        // 8-bytes length
        mkv_buffer_write_master(io, EMLB_ID_VOID, size - 9, 8);
        mkv_buffer_skip(io, size - 9);
    }
}

void mkv_write_size(const struct mkv_ioutil_t* io, uint64_t offset, uint32_t size)
{
    uint64_t offset2;
    assert(size < 0x10000000);
    offset2 = mkv_buffer_tell(io);
    mkv_buffer_seek(io, offset);
    mkv_buffer_w32(io, size | 0x10000000);
    mkv_buffer_seek(io, offset2);
}

static void mkv_write_ebml_header(struct mkv_ioutil_t *io, const struct mkv_t* mkv)
{
    uint64_t offset;
    mkv_buffer_write_master(io, EBML_ID_EBML, 27+strlen(mkv->doc), 0); // EBML
    offset = mkv_buffer_tell(io);
    mkv_buffer_write_uint_element(io, 0x4286, 1); // EBMLVersion
    mkv_buffer_write_uint_element(io, 0x42F7, 1); // EBMLReadVersion
    mkv_buffer_write_uint_element(io, 0x42F2, 4); // EBMLMaxIDLength
    mkv_buffer_write_uint_element(io, 0x42F3, 8); // EBMLMaxSizeLength
    mkv_buffer_write_string_element(io, 0x4282, mkv->doc, strlen(mkv->doc)); // DocType
    mkv_buffer_write_uint_element(io, 0x4287, 4); // DocTypeVersion
    mkv_buffer_write_uint_element(io, 0x4285, 2); // DocTypeReadVersion
    assert(27 + strlen(mkv->doc) == mkv_buffer_tell(io) - offset);
}

static void mkv_write_seek_head(struct mkv_ioutil_t *io, const struct mkv_segment_seek_t* seek, const struct mkv_writer_t* writer)
{
    size_t i, size;
    uint64_t offset;
    const uint32_t id[] = { EBML_ID_INFO, EBML_ID_TRACKS, /*EBML_ID_CHAPTERS, EBML_ID_ATTACHMENTS, EBML_ID_TAGS,*/ EBML_ID_CLUSTER, EBML_ID_CUES };
    uint64_t position[] = {seek->info-writer->seekhead_offset, seek->tracks - writer->seekhead_offset, /*seek->chapters-writer->seekhead_offset, seek->attachments-writer->seekhead_offset, seek->tags-writer->seekhead_offset,*/ seek->cluster - writer->seekhead_offset, seek->cues - writer->seekhead_offset };

    for (size = i = 0; i < sizeof(id) / sizeof(id[0]); i++)
    {
        if (position[i] < 1)
            continue;

        size += 3 /*seek*/ + 3 /*ele*/ + ebml_uint_length(id[i]) + 3 /*ele*/ + ebml_uint_length(position[i]);
    }
    assert(size < MKV_SEEK_HEAD_RESERVED + 6);

    // Segment/SeekHead
    offset = mkv_buffer_tell(io) + 4;
    mkv_buffer_write_master(io, EBML_ID_SEEK, size, 0);

    for (i = 0; i < sizeof(id) / sizeof(id[0]); i++)
    {
        if (position[i] < 1)
            continue;

        // Segment/SeekHead/Seek
        mkv_buffer_write_master(io, 0x4DBB, 3 /*ele*/ + ebml_uint_length(id[i]) + 3 /*ele*/ + ebml_uint_length(position[i]), 0);
        mkv_buffer_write_uint_element(io, 0x53AB, id[i]); // SeekID
        mkv_buffer_write_uint_element(io, 0x53AC, position[i]); // SeekPosition
    }

    assert(size+ebml_size_length(size) == mkv_buffer_tell(io) - offset);
    mkv_write_void(io, MKV_SEEK_HEAD_RESERVED - (uint32_t)(mkv_buffer_tell(io) - offset + 4)); // refill void
    offset = mkv_buffer_tell(io);
}

static void mkv_write_info(struct mkv_ioutil_t* io, struct mkv_writer_t* writer)
{
    uint64_t offset;
    uint8_t uid[16]; // 128-bits
    int live = (MKV_OPTION_LIVE & writer->options) ? 1 : 0;
    int size = 7 /*timescale*/ + 2 * (3 + strlen(MKV_APP)) + 19 /*SegmentUID*/ + (live ? 0 : 11 /*duration*/);

    // Segment/Info
    mkv_buffer_write_master(io, EBML_ID_INFO, size, 0);
    offset = mkv_buffer_tell(io);
    
    mkv_buffer_write_uint_element(io, 0x2AD7B1, writer->mkv.timescale); // TimestampScale
    mkv_buffer_write_string_element(io, 0x4D80, MKV_APP, strlen(MKV_APP)); // MuxingApp
    mkv_buffer_write_string_element(io, 0x5741, MKV_APP, strlen(MKV_APP)); // WritingApp
    mkv_buffer_write_binary_element(io, 0x73A4, uid, sizeof(uid)); // SegmentUID
    
    if (!live)
    {
        writer->info_duration_offset = mkv_buffer_tell(io);
        mkv_buffer_write_double_element(io, 0x4489, 0.0); // placeholder, Segment/Info/Duration
    }

    assert(size == mkv_buffer_tell(io) - offset);
}

static void mkv_write_tracks(struct mkv_ioutil_t* io, struct mkv_t* mkv)
{
    int i;
    uint64_t offset;

    // Segment/Tracks
    mkv_buffer_write_master(io, EBML_ID_TRACKS, 0x0FFFFFFF, 4); // placeholder
    offset = mkv_buffer_tell(io);

    for(i = 0; i < mkv->track_count; i++)
    {
        mkv_write_track(io, mkv->tracks + i);
    }

    mkv_write_size(io, offset-4, (uint32_t)(mkv_buffer_tell(io) - offset)); /* update size */
}

static void mkv_write_init(struct mkv_writer_t* writer)
{
    // Segment
    writer->segment_offset = mkv_buffer_tell(&writer->io) + 4;
    mkv_buffer_write_master(&writer->io, EBML_ID_SEGMENT, 0x00FFFFFFFFFFFFFF, 8); // placeholder
    
    if (0 == (MKV_OPTION_LIVE & writer->options))
    {
        writer->seekhead_offset = mkv_buffer_tell(&writer->io);
        mkv_write_void(&writer->io, MKV_SEEK_HEAD_RESERVED); // Void, placeholder
    }

    writer->mkv.seek.info = mkv_buffer_tell(&writer->io);
    mkv_write_info(&writer->io, writer);

    writer->mkv.seek.tracks = mkv_buffer_tell(&writer->io);
    mkv_write_tracks(&writer->io, &writer->mkv);
}

struct mkv_writer_t* mkv_writer_create(const struct mkv_buffer_t* buffer, void* param, int options)
{
    struct mkv_t* mkv;
    struct mkv_writer_t* writer;
    writer = (struct mkv_writer_t*)calloc(1, sizeof(struct mkv_writer_t));
    if (NULL == writer)
        return NULL;

    mkv = &writer->mkv;
    writer->options = options;
    writer->io.param = param;
    memcpy(&writer->io.io, buffer, sizeof(writer->io.io));

    // It is RECOMMENDED that the size of each individual Cluster Element 
    // be limited to store no more than 5 seconds or 5 megabytes.
    writer->cluster_limit_duration = 5000;
    writer->cluster_limit_size = 5000000;

    mkv->timescale = 1000000;
    snprintf(mkv->doc, sizeof(mkv->doc) - 1, "%s", (MKV_OPTION_WEBM & options) ? "webm" : "matroska");
    mkv_write_ebml_header(&writer->io, mkv);
    return writer;
}

void mkv_writer_destroy(struct mkv_writer_t* writer)
{
    int i;
    uint64_t offset;
    uint64_t cluster_size;
    struct mkv_t* mkv;
    struct mkv_track_t* track;
    struct mkv_cluster_t* cluster;
    mkv = &writer->mkv;

    // end cluster
    cluster = &writer->cluster;
    cluster_size = mkv_buffer_tell(&writer->io) - cluster->position;

    // Cues
    writer->mkv.seek.cues = mkv_buffer_tell(&writer->io);
    mkv_write_cues(&writer->io, &writer->mkv);

    if (0 == (MKV_OPTION_LIVE & writer->options))
    {
        if (cluster->position > 0 && cluster_size > 0)
        {
            assert(cluster_size > 10);
            mkv_write_size(&writer->io, cluster->position + 4 /*id*/, (uint32_t)cluster_size - 8 /*id+size*/);
        }

        // finish segment size
        offset = mkv_buffer_tell(&writer->io);
        assert(offset >= writer->segment_offset);
        mkv_buffer_seek(&writer->io, writer->segment_offset);
        mkv_buffer_w64(&writer->io, (offset - 8 /*size*/ - writer->segment_offset) | 0x0100000000000000ULL);
        mkv_buffer_seek(&writer->io, offset);

        // Meta Seek Information
        offset = mkv_buffer_tell(&writer->io);
        assert(offset >= writer->seekhead_offset);
        mkv_buffer_seek(&writer->io, writer->seekhead_offset);
        mkv_write_seek_head(&writer->io, &writer->mkv.seek, writer);
        mkv_buffer_seek(&writer->io, offset);

        // Duration
        for (i = 0; i < mkv->track_count; i++)
        {
            track = &mkv->tracks[i];
            if (track->sample_count < 1)
                continue;

            // pts in ms
            track->duration = track->last_ts - track->first_ts;
            if (track->sample_count > 1)
                track->duration += track->duration / (track->sample_count - 1); // duration += avg-duration
            if ((double)track->duration > mkv->duration)
                mkv->duration = (double)track->duration; // maximum track duration
        }

        // Segment Information -> Duration
        offset = mkv_buffer_tell(&writer->io);
        assert(offset >= writer->info_duration_offset);
        mkv_buffer_seek(&writer->io, writer->info_duration_offset);
        mkv_buffer_write_double_element(&writer->io, 0x4489, mkv->duration); // Segment/Info/Duration
        mkv_buffer_seek(&writer->io, offset);
    }

    for (i = 0; i < mkv->track_count; i++)
        mkv_track_free(mkv->tracks + i);
    
    FREE(mkv->cue.positions);
    FREE(mkv->tracks);
    free(writer);
}

int mkv_writer_write(struct mkv_writer_t* writer, int tid, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
    int r;
    int64_t ts, diff;
    int64_t duration;
    uint64_t cluster_size;
    int key_frame;
    int new_cluster;
    struct mkv_t* mkv;
    struct mkv_track_t* track;
    struct mkv_sample_t sample;
    struct mkv_cluster_t* cluster;

    assert(bytes < UINT32_MAX && bytes < UINT32_MAX - 16);
    if (tid < 1 || tid > (int)writer->mkv.track_count) // base from 1
        return -ENOENT;
    
    mkv = &writer->mkv;
    track = &mkv->tracks[tid -1];
    cluster = &writer->cluster;

    ts = writer->use_dts ? dts : pts;
    ts += writer->segment_ts;
    duration = ts - cluster->timestamp;
    
    new_cluster = 0;
    key_frame = (flags & MKV_FLAGS_KEYFRAME) && mkv_codec_is_video(track->codecid) ? 1 : 0;

    cluster_size = mkv_buffer_tell(&writer->io) - cluster->position;
    if (cluster->position < 1 || cluster_size + bytes > UINT32_MAX
        || cluster_size + bytes >= writer->cluster_limit_size
        || duration > writer->cluster_limit_duration + 30000 /*30s*/
        || (duration >= writer->cluster_limit_duration && (!writer->has_video || key_frame)))
    {
        new_cluster = 1;
    }

    if (new_cluster)
    {
        if (writer->segment_offset < 1)
        {
            mkv_write_init(writer); // mkv segment placeholder
            writer->mkv.seek.cluster = mkv_buffer_tell(&writer->io);
        }

        if (cluster->position > 0 && cluster_size > 0)
        {
            assert(cluster_size > 10);
            mkv_write_size(&writer->io, cluster->position+4 /*id*/, (uint32_t)cluster_size - 8 /*id+size*/);
        }

        writer->has_video = 0; // clear video flag
        cluster->timestamp = ts;
        cluster->prev_size = 0;
        cluster->position = mkv_buffer_tell(&writer->io); // don't with id

        // Segment/Cluster
        mkv_buffer_write_master(&writer->io, EBML_ID_CLUSTER, 0x0FFFFFFF, 4); // placeholder
        mkv_buffer_write_uint_element(&writer->io, 0xE7, ts); // Segment/Cluster/Timestamp
    }

    // fix: case on ts - cluster->timestamp < 0
    //ts = (ts - cluster->timestamp) * 1000000 / mkv->timescale;
    diff = ts * 1000000 / mkv->timescale - cluster->timestamp * 1000000 / mkv->timescale;

    memset(&sample, 0, sizeof(sample));
    sample.track = tid;
    sample.offset = mkv_buffer_tell(&writer->io);
    sample.bytes = (uint32_t)bytes;
    sample.flags = flags;
    sample.data = (void*)data;
    sample.pts = diff;
    sample.dts = diff;

    r = mkv_cluster_simple_block_write(mkv, &sample, &writer->io);
    if (r < 0)
        return r;

    if (key_frame)
    {
        r = mkv_cue_add(mkv, tid, ts, cluster->position, sample.offset-cluster->position);
        if (0 != r)
            return r;
    }

    if (0 == track->sample_count)
        track->first_ts = ts;
    track->last_ts = ts;
    track->sample_count++;

    if (!writer->has_video && mkv_codec_is_video(track->codecid))
        writer->has_video = 1;

    return mkv_buffer_error(&writer->io);
}

int mkv_writer_add_audio(struct mkv_writer_t* writer, enum mkv_codec_t codec, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
    struct mkv_t* mkv;
    struct mkv_track_t* track;

    mkv = &writer->mkv;
    track = mkv_add_track(mkv);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mkv_add_audio(track, codec, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size))
        return -ENOMEM;

    // default duation, for block lacing
    if (MKV_CODEC_AUDIO_AAC == codec)
        track->duration = 1000 * 1024 / sample_rate;

    mkv->track_count++;
    return track->id;
}

int mkv_writer_add_video(struct mkv_writer_t* writer, enum mkv_codec_t codec, int width, int height, const void* extra_data, size_t extra_data_size)
{
    struct mkv_t* mkv;
    struct mkv_track_t* track;

    mkv = &writer->mkv;
    track = mkv_add_track(mkv);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mkv_add_video(track, codec, width, height, extra_data, extra_data_size))
        return -ENOMEM;

    mkv->track_count++;
    return track->id;
}

int mkv_writer_add_subtitle(struct mkv_writer_t* writer, enum mkv_codec_t codec, const void* extra_data, size_t extra_data_size)
{
    struct mkv_t* mkv;
    struct mkv_track_t* track;

    mkv = &writer->mkv;
    track = mkv_add_track(mkv);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mkv_add_subtitle(track, codec, extra_data, extra_data_size))
        return -ENOMEM;

    mkv->track_count++;
    return track->id;
}
