#include "mkv-internal.h"

struct mkv_track_t* mkv_add_track(struct mkv_t* mkv)
{
    void* ptr = NULL;
    struct mkv_track_t* track;

    ptr = realloc(mkv->tracks, sizeof(struct mkv_track_t) * (mkv->track_count + 1));
    if (NULL == ptr) return NULL;

    mkv->tracks = (struct mkv_track_t*)ptr;
    track = &mkv->tracks[mkv->track_count];
    memset(track, 0, sizeof(struct mkv_track_t));
    track->id = mkv->track_count+1; // base from 1
    track->uid = (uint64_t)track; // TODO: uuid
    track->flag_default = 1;
    track->flag_enabled = 1;
    track->flag_lacing = 0;
    track->first_ts = INT64_MIN;
    track->last_ts = INT64_MIN;
    return track;
}

int mkv_track_free(struct mkv_track_t* track)
{
	if (track->name)
		free(track->name);
	if (track->lang)
		free(track->lang);
	if (track->codec_extra.ptr)
		free(track->codec_extra.ptr);
	return 0;
}

struct mkv_track_t* mkv_track_find(struct mkv_t* mkv, unsigned int id)
{
	int i;
	struct mkv_track_t* track;

	for (i = 0; i < mkv->track_count; i++)
	{
		track = &mkv->tracks[i];
		if (track->id == id)
			return track;
	}
	return NULL;
}

int mkv_add_audio(struct mkv_track_t* track, enum mkv_codec_t codec, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
    //track->name = mkv_codec_find_name(codec); //  don't free
    track->codecid = codec;
    track->media = MKV_TRACK_AUDIO;
    track->u.audio.channels = (unsigned int)channel_count;
    track->u.audio.bits = (unsigned int)bits_per_sample;
    track->u.audio.sampling = sample_rate;

    if (extra_data_size > 0)
    {
        track->codec_extra.ptr = malloc(extra_data_size);
        if (NULL == track->codec_extra.ptr)
            return -ENOMEM;
        memcpy(track->codec_extra.ptr, extra_data, extra_data_size);
        track->codec_extra.len = (int)extra_data_size;
    }
    return 0;
}

int mkv_add_video(struct mkv_track_t* track, enum mkv_codec_t codec, int width, int height, const void* extra_data, size_t extra_data_size)
{
    //track->name = mkv_codec_find_name(codec); //  don't free
    track->codecid = codec;
    track->media = MKV_TRACK_VIDEO;
    track->u.video.width = width;
    track->u.video.height = height;

    if (extra_data_size > 0)
    {
        track->codec_extra.ptr = malloc(extra_data_size);
        if (NULL == track->codec_extra.ptr)
            return -ENOMEM;
        memcpy(track->codec_extra.ptr, extra_data, extra_data_size);
        track->codec_extra.len = (int)extra_data_size;
    }
    return 0;
}

int mkv_add_subtitle(struct mkv_track_t* track, enum mkv_codec_t codec, const void* extra_data, size_t extra_data_size)
{
    //track->name = mkv_codec_find_name(codec);  //  don't free
    track->codecid = codec;
    track->media = MKV_TRACK_SUBTITLE;

    if (extra_data_size > 0)
    {
        track->codec_extra.ptr = malloc(extra_data_size);
        if (NULL == track->codec_extra.ptr)
            return -ENOMEM;
        memcpy(track->codec_extra.ptr, extra_data, extra_data_size);
        track->codec_extra.len = (int)extra_data_size;
    }
    return 0;
}

int mkv_write_track(struct mkv_ioutil_t* io, struct mkv_track_t* track)
{
    uint64_t offset;
    const char* codec;

    codec = mkv_codec_find_name(track->codecid);
    if (!codec || !*codec)
    {
        assert(0);
        return -1;
    }

    // Segment/Tracks/TrackEntry
    mkv_buffer_write_master(io, 0xAE, 0x0FFFFFFF, 4); // placeholder
    offset = mkv_buffer_tell(io);

    mkv_buffer_write_uint_element(io, 0xD7, track->id); // TrackNumber
    mkv_buffer_write_uint_element(io, 0x73C5, track->uid); // TrackUID
    mkv_buffer_write_uint_element(io, 0x83, track->media); // TrackType
    mkv_buffer_write_string_element(io, 0x86, codec, strlen(codec)); // CodecID
    if (track->codec_extra.len > 0)
        mkv_buffer_write_binary_element(io, 0x63A2, track->codec_extra.ptr, track->codec_extra.len); // CodecPrivate

    mkv_buffer_write_uint_element(io, 0x9C, track->flag_lacing); // FlagLacing
    if(track->flag_enabled != 1)
        mkv_buffer_write_uint_element(io, 0xB9, track->flag_enabled); // FlagEnabled
    if (track->flag_default != 1)
        mkv_buffer_write_uint_element(io, 0x88, track->flag_default); // FlagDefault
    if (track->flag_forced != 0)
        mkv_buffer_write_uint_element(io, 0x55AA, track->flag_forced); // FlagForced

    if (track->duration > 0)
        mkv_buffer_write_uint_element(io, 0x23E383, track->duration); // DefaultDuration

    if (MKV_TRACK_VIDEO == track->media)
    {
        // Segment/Tracks/TrackEntry/Video
        mkv_buffer_write_master(io, 0xE0, 2 + ebml_uint_length(track->u.video.width) + 2 + ebml_uint_length(track->u.video.height), 0);
        mkv_buffer_write_uint_element(io, 0xB0, track->u.video.width); // PixelWidth
        mkv_buffer_write_uint_element(io, 0xBA, track->u.video.height); // PixelHeight

        // TODO: alpha mode
    }
    else if (MKV_TRACK_AUDIO == track->media)
    {
        if (MKV_CODEC_AUDIO_OPUS == track->codecid)
            mkv_buffer_write_uint_element(io, 0x56BB, 80000000); // SeekPreRoll

        // Segment/Tracks/TrackEntry/Audio
        mkv_buffer_write_master(io, 0xE1, 3 /*channels*/ + 4 /*bits*/ + 10 /*sampling*/, 0);
        mkv_buffer_write_uint_element(io, 0x9F, track->u.audio.channels); // Channels
        mkv_buffer_write_uint_element(io, 0x6264, track->u.audio.bits); // BitDepth
        mkv_buffer_write_double_element(io, 0xB5, track->u.audio.sampling); // SamplingFrequency
    }
    else if (MKV_TRACK_SUBTITLE == track->media)
    {
    }
    else
    {
        assert(0);
    }

    mkv_write_size(io, offset - 4, (uint32_t)(mkv_buffer_tell(io) - offset)); /* update size */
    return 0;
}
