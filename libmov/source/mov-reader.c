#include "mov-reader.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MOV_NULL MOV_TAG(0, 0, 0, 0)

#define AV_TRACK_TIMEBASE 1000

//#define MOV_READER_BOX_TREE 1
//#define MOV_READER_FMP4_FAST 1

#define MOV_READER_FLAG_FMP4_FAST 0x01

struct mov_reader_t
{
	int flags;
	int have_read_mfra;
	
	struct mov_t mov;
};

#define MOV_READER_FROM_MOV(ptr) ((struct mov_reader_t*)((char*)(ptr)-(ptrdiff_t)(&((struct mov_reader_t*)0)->mov)))

struct mov_parse_t
{
	uint32_t type;
	uint32_t parent;
	int(*parse)(struct mov_t* mov, const struct mov_box_t* box);
};

static int mov_stss_seek(struct mov_track_t* track, int64_t *timestamp);
static int mov_sample_seek(struct mov_track_t* track, int64_t timestamp);

// 8.1.1 Media Data Box (p28)
static int mov_read_mdat(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_skip(&mov->io, box->size);
	return mov_buffer_error(&mov->io);
}

// 8.1.2 Free Space Box (p28)
static int mov_read_free(struct mov_t* mov, const struct mov_box_t* box)
{
	// Container: File or other box
	mov_buffer_skip(&mov->io, box->size);
	return mov_buffer_error(&mov->io);
}

//static struct mov_sample_entry_t* mov_track_stsd_find(struct mov_track_t* track, uint32_t sample_description_index)
//{
//    if (sample_description_index > 0 && sample_description_index <= track->stsd.entry_count)
//        return &track->stsd.entries[sample_description_index-1];
//    return NULL;
//}

static int mov_index_build(struct mov_track_t* track)
{
	void* p;
	uint32_t i, j;
	struct mov_stbl_t* stbl = &track->stbl;

	if (stbl->stss_count > 0 || MOV_VIDEO != track->handler_type)
		return 0;

	for (i = 0; i < track->sample_count; i++)
	{
		if (track->samples[i].flags & MOV_AV_FLAG_KEYFREAME)
			++stbl->stss_count;
	}

	p = realloc(stbl->stss, sizeof(stbl->stss[0]) * stbl->stss_count);
	if (!p) return ENOMEM;
	stbl->stss = p;

	for (j = i = 0; i < track->sample_count && j < stbl->stss_count; i++)
	{
		if (track->samples[i].flags & MOV_AV_FLAG_KEYFREAME)
			stbl->stss[j++] = i + 1; // uint32_t sample_number, start from 1
	}
	assert(j == stbl->stss_count);
	return 0;
}

// 8.3.1 Track Box (p31)
// Box Type : 'trak' 
// Container : Movie Box('moov') 
// Mandatory : Yes 
// Quantity : One or more
static int mov_read_trak(struct mov_t* mov, const struct mov_box_t* box)
{
	int r;

    mov->track = NULL;
	r = mov_reader_box(mov, box);
	if (0 == r)
	{
        mov->track->tfdt_dts = 0;
        if (mov->track->sample_count > 0)
        {
            mov_apply_stco(mov->track);
            mov_apply_elst(mov->track);
            mov_apply_stts(mov->track);
            mov_apply_ctts(mov->track);
			mov_apply_stss(mov->track);

            mov->track->tfdt_dts = mov->track->samples[mov->track->sample_count - 1].dts;
        }
	}

	return r;
}

static int mov_read_dref(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	for (i = 0; i < entry_count; i++)
	{
		uint32_t size = mov_buffer_r32(&mov->io);
		/*uint32_t type = */mov_buffer_r32(&mov->io);
		/*uint32_t vern = */mov_buffer_r32(&mov->io); /* version + flags */
		mov_buffer_skip(&mov->io, size-12);
	}

	(void)box;
	return 0;
}

static int mov_read_btrt(struct mov_t* mov, const struct mov_box_t* box)
{
	// ISO/IEC 14496-15:2010(E)
	// 5.3.4 AVC Video Stream Definition (p19)
	mov_buffer_r32(&mov->io); /* bufferSizeDB */
	mov_buffer_r32(&mov->io); /* maxBitrate */
	mov_buffer_r32(&mov->io); /* avgBitrate */
	(void)box;
	return 0;
}

static int mov_read_uuid(struct mov_t* mov, const struct mov_box_t* box)
{
	uint8_t usertype[16] = { 0 };
	if(box->size > 16) 
	{
		mov_buffer_read(&mov->io, usertype, sizeof(usertype));
		mov_buffer_skip(&mov->io, box->size - 16);
	}
	return mov_buffer_error(&mov->io);
}

static int mov_read_moof(struct mov_t* mov, const struct mov_box_t* box)
{
    // 8.8.7 Track Fragment Header Box (p71)
    // If base-data-offset-present not provided and if the default-base-is-moof flag is not set,
    // the base-data-offset for the first track in the movie fragment is the position of
    // the first byte of the enclosing Movie Fragment Box, for second and subsequent track fragments, 
    // the default is the end of the data defined by the preceding track fragment.
	mov->moof_offset = mov->implicit_offset = mov_buffer_tell(&mov->io) - 8 /*box size */;
	return mov_reader_box(mov, box);
}

static int mov_read_mfra(struct mov_t* mov, const struct mov_box_t* box)
{
	int r;
	struct mov_reader_t* reader;
	reader = MOV_READER_FROM_MOV(mov);
	r = mov_reader_box(mov, box);
	reader->have_read_mfra = 1;
	return r;
}

// 8.8.11 Movie Fragment Random Access Offset Box (p75)
static int mov_read_mfro(struct mov_t* mov, const struct mov_box_t* box)
{
	(void)box;
	mov_buffer_r32(&mov->io); /* version & flags */
	mov->mfro = mov_buffer_r32(&mov->io); /* size */
	return mov_buffer_error(&mov->io);
}

int mov_reader_root(struct mov_t* mov)
{
	struct mov_box_t box;

	box.type = MOV_ROOT;
	box.size = UINT64_MAX;
#if defined(DEBUG) || defined(_DEBUG)
	box.level = 0;
#endif

	return mov_reader_box(mov, &box);
}

static int mov_read_default(struct mov_t* mov, const struct mov_box_t* box)
{
	return mov_reader_box(mov, box);
}

static struct mov_parse_t s_mov_parse_table[] = {
	{ MOV_TAG('a', 'v', '1', 'C'), MOV_NULL, mov_read_av1c }, // av1-isobmff
	{ MOV_TAG('a', 'v', 'c', 'C'), MOV_NULL, mov_read_avcc }, // ISO/IEC 14496-15:2010(E) avcC
	{ MOV_TAG('b', 't', 'r', 't'), MOV_NULL, mov_read_btrt }, // ISO/IEC 14496-15:2010(E) 5.3.4.1.1 Definition
	{ MOV_TAG('c', 'o', '6', '4'), MOV_STBL, mov_read_stco },
    { MOV_TAG('C', 'o', 'L', 'L'), MOV_STBL, mov_read_coll },
	{ MOV_TAG('c', 't', 't', 's'), MOV_STBL, mov_read_ctts },
	{ MOV_TAG('c', 's', 'l', 'g'), MOV_STBL, mov_read_cslg },
	{ MOV_TAG('d', 'i', 'n', 'f'), MOV_MINF, mov_read_default },
    { MOV_TAG('d', 'O', 'p', 's'), MOV_NULL, mov_read_dops },
	{ MOV_TAG('d', 'r', 'e', 'f'), MOV_DINF, mov_read_dref },
	{ MOV_TAG('e', 'd', 't', 's'), MOV_TRAK, mov_read_default },
	{ MOV_TAG('e', 'l', 's', 't'), MOV_EDTS, mov_read_elst },
	{ MOV_TAG('e', 's', 'd', 's'), MOV_NULL, mov_read_esds }, // ISO/IEC 14496-14:2003(E) mp4a/mp4v/mp4s
	{ MOV_TAG('f', 'r', 'e', 'e'), MOV_NULL, mov_read_free },
	{ MOV_TAG('f', 't', 'y', 'p'), MOV_ROOT, mov_read_ftyp },
	{ MOV_TAG('g', 'm', 'i', 'n'), MOV_GMHD, mov_read_gmin }, // Apple QuickTime gmin
	{ MOV_TAG('g', 'm', 'h', 'd'), MOV_MINF, mov_read_default }, // Apple QuickTime gmhd
	{ MOV_TAG('h', 'd', 'l', 'r'), MOV_MDIA, mov_read_hdlr }, // Apple QuickTime minf also has hdlr
	{ MOV_TAG('h', 'v', 'c', 'C'), MOV_NULL, mov_read_hvcc }, // ISO/IEC 14496-15:2014 hvcC
	{ MOV_TAG('l', 'e', 'v', 'a'), MOV_MVEX, mov_read_leva },
	{ MOV_TAG('m', 'd', 'a', 't'), MOV_ROOT, mov_read_mdat },
	{ MOV_TAG('m', 'd', 'h', 'd'), MOV_MDIA, mov_read_mdhd },
	{ MOV_TAG('m', 'd', 'i', 'a'), MOV_TRAK, mov_read_default },
	{ MOV_TAG('m', 'e', 'h', 'd'), MOV_MVEX, mov_read_mehd },
	{ MOV_TAG('m', 'f', 'h', 'd'), MOV_MOOF, mov_read_mfhd },
	{ MOV_TAG('m', 'f', 'r', 'a'), MOV_ROOT, mov_read_mfra },
	{ MOV_TAG('m', 'f', 'r', 'o'), MOV_MFRA, mov_read_mfro },
	{ MOV_TAG('m', 'i', 'n', 'f'), MOV_MDIA, mov_read_default },
	{ MOV_TAG('m', 'o', 'o', 'v'), MOV_ROOT, mov_read_default },
	{ MOV_TAG('m', 'o', 'o', 'f'), MOV_ROOT, mov_read_moof },
	{ MOV_TAG('m', 'v', 'e', 'x'), MOV_MOOV, mov_read_default },
	{ MOV_TAG('m', 'v', 'h', 'd'), MOV_MOOV, mov_read_mvhd },
	{ MOV_TAG('n', 'm', 'h', 'd'), MOV_MINF, mov_read_nmhd }, // ISO/IEC 14496-12:2015(E) 8.4.5.2 Null Media Header Box (p45)
	{ MOV_TAG('p', 'a', 's', 'p'), MOV_NULL, mov_read_pasp },
	{ MOV_TAG('s', 'i', 'd', 'x'), MOV_ROOT, mov_read_sidx },
	{ MOV_TAG('s', 'k', 'i', 'p'), MOV_NULL, mov_read_free },
    { MOV_TAG('S', 'm', 'D', 'm'), MOV_MINF, mov_read_smdm },
	{ MOV_TAG('s', 'm', 'h', 'd'), MOV_MINF, mov_read_smhd },
	{ MOV_TAG('s', 't', 'b', 'l'), MOV_MINF, mov_read_default },
	{ MOV_TAG('s', 't', 'c', 'o'), MOV_STBL, mov_read_stco },
//	{ MOV_TAG('s', 't', 'h', 'd'), MOV_MINF, mov_read_default }, // ISO/IEC 14496-12:2015(E) 12.6.2 Subtitle media header (p185)
	{ MOV_TAG('s', 't', 's', 'c'), MOV_STBL, mov_read_stsc },
	{ MOV_TAG('s', 't', 's', 'd'), MOV_STBL, mov_read_stsd },
	{ MOV_TAG('s', 't', 's', 's'), MOV_STBL, mov_read_stss },
	{ MOV_TAG('s', 't', 's', 'z'), MOV_STBL, mov_read_stsz },
	{ MOV_TAG('s', 't', 't', 's'), MOV_STBL, mov_read_stts },
	{ MOV_TAG('s', 't', 'z', '2'), MOV_STBL, mov_read_stz2 },
	{ MOV_TAG('t', 'e', 'x', 't'), MOV_GMHD, mov_read_text },
	{ MOV_TAG('t', 'f', 'd', 't'), MOV_TRAF, mov_read_tfdt },
	{ MOV_TAG('t', 'f', 'h', 'd'), MOV_TRAF, mov_read_tfhd },
	{ MOV_TAG('t', 'f', 'r', 'a'), MOV_MFRA, mov_read_tfra },
	{ MOV_TAG('t', 'k', 'h', 'd'), MOV_TRAK, mov_read_tkhd },
	{ MOV_TAG('t', 'r', 'a', 'k'), MOV_MOOV, mov_read_trak },
	{ MOV_TAG('t', 'r', 'e', 'x'), MOV_MVEX, mov_read_trex },
	{ MOV_TAG('t', 'r', 'a', 'f'), MOV_MOOF, mov_read_default },
	{ MOV_TAG('t', 'r', 'u', 'n'), MOV_TRAF, mov_read_trun },
	{ MOV_TAG('u', 'd', 't', 'a'), MOV_MOOV, mov_read_udta },
	{ MOV_TAG('u', 'u', 'i', 'd'), MOV_NULL, mov_read_uuid },
	{ MOV_TAG('v', 'm', 'h', 'd'), MOV_MINF, mov_read_vmhd },
    { MOV_TAG('v', 'p', 'c', 'C'), MOV_NULL, mov_read_vpcc },

	{ 0, 0, NULL } // last
};

int mov_reader_box(struct mov_t* mov, const struct mov_box_t* parent)
{
	int i;
	uint64_t bytes = 0;
	struct mov_box_t box;
	struct mov_reader_t* reader;
	int (*parse)(struct mov_t* mov, const struct mov_box_t* box);

	reader = MOV_READER_FROM_MOV(mov);
	while (bytes + 8 < parent->size && 0 == mov_buffer_error(&mov->io))
	{
		uint64_t n = 8;
		box.size = mov_buffer_r32(&mov->io);
		box.type = mov_buffer_r32(&mov->io);
		
#if defined(MOV_READER_BOX_TREE) && !defined(NDEBUG)
		box.level = parent->level + 1;
		for (i = 0; i < parent->level; i++)
			printf("\t");
		printf("%c%c%c%c, size: %d\n", (char)(box.type >> 24), (char)(box.type >> 16), (char)(box.type >> 8), (char)box.type, (int)box.size);
#endif

		if (1 == box.size)
		{
			// unsigned int(64) large size
			box.size = mov_buffer_r64(&mov->io);
			n += 8;
		}
		else if (0 == box.size)
		{
			if (0 == box.type)
				return 0; // all done
			box.size = UINT64_MAX;
		}

		if (UINT64_MAX == box.size)
		{
			bytes = parent->size;
		}
		else
		{
			bytes += box.size;
			box.size -= n;
		}

		if (bytes > parent->size)
			return -1;

		for (i = 0, parse = NULL; s_mov_parse_table[i].type && !parse; i++)
		{
			if (s_mov_parse_table[i].type == box.type)
			{
				// Apple QuickTime minf also has hdlr
				if(!s_mov_parse_table[i].parent || MOV_ROOT == parent->type || s_mov_parse_table[i].parent == parent->type)
					parse = s_mov_parse_table[i].parse;
			}
		}

		if (NULL == parse)
		{
			mov_buffer_skip(&mov->io, box.size);
		}
		else
		{
			int r;
			uint64_t pos, pos2;
			pos = mov_buffer_tell(&mov->io);
			r = parse(mov, &box);
			assert(0 == r || mov_buffer_error(&mov->io));
			if (0 != r) return r;
			pos2 = mov_buffer_tell(&mov->io);
			assert(pos2 - pos == box.size);
			mov_buffer_skip(&mov->io, box.size - (pos2 - pos));
		}

		// fmp4: read one-fragment only
		if ((reader->flags & MOV_READER_FLAG_FMP4_FAST) && MOV_TAG('m', 'o', 'o', 'f') == box.type)
		{
			if (!reader->have_read_mfra)
			{
				mov_fragment_seek_read_mfra(mov);
				reader->have_read_mfra = 1; // force, seek once only
			}

			// skip fast mode, fallback to read all
			if(mov->mfro > 0)
				break;
		}
	}

	return mov_buffer_error(&mov->io) ? -1 : 0;
}

static int mov_reader_init(struct mov_reader_t* reader)
{
	int i, r;
	struct mov_t* mov;
	struct mov_track_t* track;

	mov = &reader->mov;
	r = mov_reader_root(mov);
//	if (0 != r) return r;  // ignore file read error(for streaming file)

	for (i = 0; i < mov->track_count; i++)
	{
		track = mov->tracks + i;
		mov_index_build(track);
		//track->sample_offset = 0; // reset
		
		// fragment mp4
		if (0 == track->mdhd.duration && track->sample_count > 0)
			track->mdhd.duration = track->samples[track->sample_count - 1].dts - track->samples[0].dts;
		if (0 == track->tkhd.duration)
			track->tkhd.duration = track->mdhd.duration * mov->mvhd.timescale / track->mdhd.timescale;
		if (track->tkhd.duration > mov->mvhd.duration)
			mov->mvhd.duration = track->tkhd.duration; // maximum track duration
	}

	return 0;
}

struct mov_reader_t* mov_reader_create(const struct mov_buffer_t* buffer, void* param)
{
	struct mov_reader_t* reader;
	reader = (struct mov_reader_t*)calloc(1, sizeof(*reader));
	if (NULL == reader)
		return NULL;

#if defined(MOV_READER_FMP4_FAST)
	reader->flags |= MOV_READER_FLAG_FMP4_FAST;
#endif

	// ISO/IEC 14496-12:2012(E) 4.3.1 Definition (p17)
	// Files with no file-type box should be read as if they contained an FTYP box 
	// with Major_brand='mp41', minor_version=0, and the single compatible brand 'mp41'.
	reader->mov.ftyp.major_brand = MOV_BRAND_MP41;
	reader->mov.ftyp.minor_version = 0;
	reader->mov.ftyp.brands_count = 0;
	reader->mov.header = 0;

	reader->mov.io.param = param;
	memcpy(&reader->mov.io.io, buffer, sizeof(reader->mov.io.io));
	if (0 != mov_reader_init(reader))
	{
		mov_reader_destroy(reader);
		return NULL;
	}
	return reader;
}

void mov_reader_destroy(struct mov_reader_t* reader)
{
	int i;
	for (i = 0; i < reader->mov.track_count; i++)
        mov_free_track(reader->mov.tracks + i);
    if (reader->mov.tracks)
        free(reader->mov.tracks);
	free(reader);
}

static struct mov_track_t* mov_reader_next(struct mov_reader_t* reader)
{
	int i;
	int64_t dts, best_dts = 0;
	struct mov_track_t* track = NULL;
	struct mov_track_t* track2;

	for (i = 0; i < reader->mov.track_count; i++)
	{
		track2 = &reader->mov.tracks[i];
		assert(track2->sample_offset <= track2->sample_count);
		if (track2->sample_offset >= track2->sample_count)
			continue;

		dts = track2->samples[track2->sample_offset].dts * 1000 / track2->mdhd.timescale;
		//if (NULL == track || dts < best_dts)
		//if (NULL == track || track->samples[track->sample_offset].offset > track2->samples[track2->sample_offset].offset)
		if (NULL == track || (dts < best_dts && best_dts - dts > AV_TRACK_TIMEBASE) || track2->samples[track2->sample_offset].offset < track->samples[track->sample_offset].offset)
		{
			track = track2;
			best_dts = dts;
		}
	}

	return track;
}

int mov_reader_read2(struct mov_reader_t* reader, mov_reader_onread2 onread, void* param)
{
	void* ptr;
	struct mov_track_t* track;
	struct mov_sample_t* sample;

FMP4_NEXT_FRAGMENT:
	track = mov_reader_next(reader);
	if (NULL == track || 0 == track->mdhd.timescale)
	{
		if ((MOV_READER_FLAG_FMP4_FAST & reader->flags) && reader->have_read_mfra
			&& 0 == mov_fragment_read_next_moof(&reader->mov))
		{
			goto FMP4_NEXT_FRAGMENT;
		}
		return 0; // EOF
	}

	assert(track->sample_offset < track->sample_count);
	sample = &track->samples[track->sample_offset];
	assert(sample->sample_description_index > 0);
	ptr = onread(param, track->tkhd.track_ID, /*sample->sample_description_index-1,*/ sample->bytes, sample->pts * 1000 / track->mdhd.timescale, sample->dts * 1000 / track->mdhd.timescale, sample->flags);
	if(!ptr)
		return ENOMEM;

	mov_buffer_seek(&reader->mov.io, sample->offset);
	mov_buffer_read(&reader->mov.io, ptr, sample->bytes);
	if (mov_buffer_error(&reader->mov.io))
	{
		// TODO: user free buffer
		return mov_buffer_error(&reader->mov.io);
	}

	track->sample_offset++; //mark as read
	return 1;
}

static void* mov_reader_read_helper(void* param, uint32_t track, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	struct mov_sample_t* sample;
	sample = (struct mov_sample_t*)param;
	if (sample->bytes < bytes)
		return NULL;
	
	sample->pts = pts;
	sample->dts = dts;
	sample->flags = flags;
	sample->bytes = (uint32_t)bytes;
	sample->sample_description_index = track;
	return sample->data;
}

int mov_reader_read(struct mov_reader_t* reader, void* buffer, size_t bytes, mov_reader_onread onread, void* param)
{
	int r;
	struct mov_sample_t sample; // temp
	//memset(&sample, 0, sizeof(sample));
	sample.data = buffer;
	sample.bytes =  (uint32_t)bytes;
	r = mov_reader_read2(reader, mov_reader_read_helper, &sample);
	if (r <= 0)
		return r;

	onread(param, sample.sample_description_index, buffer, sample.bytes, sample.pts, sample.dts, sample.flags);
	return 1;
}

int mov_reader_seek(struct mov_reader_t* reader, int64_t* timestamp)
{
	int i;
	struct mov_track_t* track;

	if (reader->have_read_mfra && (MOV_READER_FLAG_FMP4_FAST & reader->flags)
		&& reader->mov.track_count > 0 && reader->mov.tracks[0].frag_count > 0)
		return mov_fragment_seek(&reader->mov, timestamp);

	// seek video track(s)
	for (i = 0; i < reader->mov.track_count; i++)
	{
		track = &reader->mov.tracks[i];
		if (MOV_VIDEO == track->handler_type && track->stbl.stss_count > 0)
		{
			if (0 != mov_stss_seek(track, timestamp))
				return -1;
		}
	}

	// seek other track(s)
	for (i = 0; i < reader->mov.track_count; i++)
	{
		track = &reader->mov.tracks[i];
		if (MOV_VIDEO == track->handler_type && track->stbl.stss_count > 0)
			continue; // seek done

		mov_sample_seek(track, *timestamp);
	}

	return 0;
}

int mov_reader_getinfo(struct mov_reader_t* reader, struct mov_reader_trackinfo_t *ontrack, void* param)
{
	int i;
	uint32_t j;
	struct mov_track_t* track;
    struct mov_sample_entry_t* entry;

	for (i = 0; i < reader->mov.track_count; i++)
	{
		track = &reader->mov.tracks[i];
		for (j = 0; j < track->stsd.entry_count && j < 1 /* only the first */; j++)
		{
            entry = &track->stsd.entries[j];
			switch (track->handler_type)
			{
			case MOV_VIDEO:
				if(ontrack->onvideo) ontrack->onvideo(param, track->tkhd.track_ID, entry->object_type_indication, entry->u.visual.width, entry->u.visual.height, entry->extra_data, entry->extra_data_size);
				break;

			case MOV_AUDIO:
				if (ontrack->onaudio) ontrack->onaudio(param, track->tkhd.track_ID, entry->object_type_indication, entry->u.audio.channelcount, entry->u.audio.samplesize, entry->u.audio.samplerate >> 16, entry->extra_data, entry->extra_data_size);
				break;

			case MOV_SUBT:
			case MOV_TEXT:
			case MOV_SBTL:
				if (ontrack->onsubtitle) ontrack->onsubtitle(param, track->tkhd.track_ID, entry->object_type_indication, entry->extra_data, entry->extra_data_size);
				break;

			default:
				break;
			}
		}	
	}
	return 0;
}

uint64_t mov_reader_getduration(struct mov_reader_t* reader)
{
	return 0 != reader->mov.mvhd.timescale ? reader->mov.mvhd.duration * 1000 / reader->mov.mvhd.timescale : 0;
}

#define DIFF(a, b) ((a) > (b) ? ((a) - (b)) : ((b) - (a)))

static int mov_stss_seek(struct mov_track_t* track, int64_t *timestamp)
{
	int64_t clock;
	size_t start, end, mid;
	size_t idx, prev, next;
	struct mov_sample_t* sample;

	idx = mid = start = 0;
	end = track->stbl.stss_count;
	assert(track->stbl.stss_count > 0);
	clock = *timestamp * track->mdhd.timescale / 1000; // mvhd timescale

	while (start < end)
	{
		mid = (start + end) / 2;
		idx = track->stbl.stss[mid];

		if (idx < 1 || idx > track->sample_count)
		{
			// start from 1
			assert(0);
			return -1;
		}
		idx -= 1;
		sample = &track->samples[idx];
		
		if (sample->dts > clock)
			end = mid;
		else if (sample->dts < clock)
			start = mid + 1;
		else
			break;
	}

	prev = track->stbl.stss[mid > 0 ? mid - 1 : mid] - 1;
	next = track->stbl.stss[mid + 1 < track->stbl.stss_count ? mid + 1 : mid] - 1;
	if (DIFF(track->samples[prev].dts, clock) < DIFF(track->samples[idx].dts, clock))
		idx = prev;
	if (DIFF(track->samples[next].dts, clock) < DIFF(track->samples[idx].dts, clock))
		idx = next;

	*timestamp = track->samples[idx].dts * 1000 / track->mdhd.timescale;
	track->sample_offset = idx;
	return 0;
}

static int mov_sample_seek(struct mov_track_t* track, int64_t timestamp)
{
	size_t prev, next;
	size_t start, end, mid;
	struct mov_sample_t* sample;

	if (track->sample_count < 1)
		return -1;

	sample = NULL;
	mid = start = 0;
	end = track->sample_count;
	timestamp = timestamp * track->mdhd.timescale / 1000; // mvhd timecale

	while (start < end)
	{
		mid = (start + end) / 2;
		sample = track->samples + mid;
		
		if (sample->dts > timestamp)
			end = mid;
		else if (sample->dts < timestamp)
			start = mid + 1;
		else
			break;
	}

	prev = mid > 0 ? mid - 1 : mid;
	next = mid + 1 < track->sample_count ? mid + 1 : mid;
	if (DIFF(track->samples[prev].dts, timestamp) < DIFF(track->samples[mid].dts, timestamp))
		mid = prev;
	if (DIFF(track->samples[next].dts, timestamp) < DIFF(track->samples[mid].dts, timestamp))
		mid = next;

	track->sample_offset = mid;
	return 0;
}
