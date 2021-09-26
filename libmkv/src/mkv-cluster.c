#include "mkv-internal.h"
#include "mkv-buffer.h"

enum
{
	MKV_LACING_NONE = 0,
	MKV_LACING_XIPH = 1,
	MKV_LACING_EBML = 3,
	MKV_LACING_FIXED_SIZE = 2,
};

static int mkv_alloc_samples(struct mkv_t* mkv, int n)
{
	void* p;
	if (n + mkv->count <= mkv->capacity)
		return 0;

	p = realloc(mkv->samples, sizeof(struct mkv_sample_t) * (mkv->count + n+1));
	if(!p)
		return -ENOMEM;

	mkv->samples = (struct mkv_sample_t*)p;
	mkv->capacity = mkv->count + n + 1;
	return 0;
}

static int mkv_add_rap(struct mkv_t* mkv, int i)
{
	void* p;
	if (mkv->rap.count >= mkv->rap.capacity)
	{
		p = realloc(mkv->rap.raps, sizeof(mkv->rap.raps[0]) * (mkv->rap.capacity * 4 / 3 + 16));
		if (!p) return -ENOMEM;
		mkv->rap.raps = (int*)p;
		mkv->rap.capacity = mkv->rap.capacity * 4 / 3 + 16;
	}

	mkv->rap.raps[mkv->rap.count++] = i;
	return 0;
}

int mkv_cluster_simple_block_read(struct mkv_t* mkv, struct mkv_cluster_t* cluster, struct mkv_ioutil_t *io, int64_t bytes)
{
	int r;
	int tid;
	uint8_t i, j, n, f;
	uint8_t flags;
	uint64_t pos, pos2;
	int64_t size, total;
	int64_t lacing;
	uint64_t duration;
	int16_t timestamp;
	struct mkv_track_t* track;

	//if (0 != mkv_realloc(&cluster->blocks, cluster->count, &cluster->capacity, sizeof(struct mkv_block_t), 64))
	//	return -ENOMEM;
	//block = &cluster->blocks[cluster->count++];

	// https://www.matroska.org/technical/basics.html#block-structure
	pos = mkv_buffer_tell(io);
	tid = (int)mkv_buffer_read_size(io);
	timestamp = (int16_t)mkv_buffer_read_int(io, 2);
	flags = (uint8_t)mkv_buffer_read_uint(io, 1);
	total = 0; // Block Header

	track = mkv_track_find(mkv, tid);
	if (!track)
		return -1;

	// Number of nanoseconds (not scaled via TimestampScale) per frame 
	// (track->duration / 1000000000 -> seconds) * 1000000000 / timescale
	duration = track->duration / mkv->timescale;

	f = 0;
	if (flags & 0x80) { f |= MKV_FLAGS_KEYFRAME; }
	if (flags & 0x08) { f |= MKV_FLAGS_INVISIBLE; }
	if (flags & 0x01) { f |= MKV_FLAGS_DISCARDABLE; }

	switch ((flags >> 1) & 0x03) // lacing
	{
	case MKV_LACING_XIPH: // Xiph lacing
		n = (uint8_t)mkv_buffer_read_uint(io, 1);
		r = mkv_alloc_samples(mkv, n + 1);
		if (0 != r) return r;

		for (i = 0; i < n; i++)
		{
			size = 0;
			do
			{
				j = (uint8_t)mkv_buffer_read_uint(io, 1);
				size += j;
			} while (j == 255);

			mkv->samples[mkv->count + i].bytes = (uint32_t)size;
			total += size; // remain
		}

		pos2 = mkv_buffer_tell(io);
		if (total + (int64_t)(pos2 - pos) > bytes)
		{
			assert(0);
			return -1;
		}

		// last-frame(n+1)
		mkv->samples[mkv->count + n].bytes = (uint32_t)(bytes - total - (pos2 - pos));

		mkv->samples[mkv->count + 0].flags = f;
		mkv->samples[mkv->count + 0].track = tid;
		mkv->samples[mkv->count + 0].pts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].dts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].offset = mkv_buffer_tell(io);
		for (i = 1; i < n + 1; i++)
		{
			//mkv->samples[i].flags = mkv->samples[0].flags;
			mkv->samples[mkv->count + i].track = mkv->samples[mkv->count + 0].track;
			mkv->samples[mkv->count + i].pts = mkv->samples[mkv->count + i - 1].pts + duration;
			mkv->samples[mkv->count + i].dts = mkv->samples[mkv->count + i - 1].dts + duration;
			mkv->samples[mkv->count + i].offset = mkv->samples[mkv->count + i - 1].offset + mkv->samples[mkv->count + i - 1].bytes;
		}

		mkv->count += n + 1;
		break;

	case MKV_LACING_FIXED_SIZE: // fixed-size 
		n = (uint8_t)mkv_buffer_read_uint(io, 1);
		r = mkv_alloc_samples(mkv, n + 1);
		if (0 != r) return r;

		size = (bytes - 5) / (n + 1);
		assert(size * (n + 1) == bytes - 5);

		mkv->samples[mkv->count + 0].flags = f;
		mkv->samples[mkv->count + 0].track = tid;
		mkv->samples[mkv->count + 0].pts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].dts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].bytes = (uint32_t)size;
		mkv->samples[mkv->count + 0].offset = mkv_buffer_tell(io);
		for (i = 1; i < n + 1; i++)
		{
			//mkv->samples[mkv->count + i].flags = mkv->samples[mkv->count + 0].flags;
			mkv->samples[mkv->count + i].track = mkv->samples[mkv->count + 0].track;
			mkv->samples[mkv->count + i].pts = mkv->samples[mkv->count + i - 1].pts + duration;
			mkv->samples[mkv->count + i].dts = mkv->samples[mkv->count + i - 1].dts + duration;
			mkv->samples[mkv->count + i].bytes = (uint32_t)size;
			mkv->samples[mkv->count + i].offset = mkv->samples[mkv->count + i - 1].offset + mkv->samples[mkv->count + i - 1].bytes;
		}

		mkv->count += n + 1;
		break;

	case MKV_LACING_EBML: // EBML lacing
		n = (uint8_t)mkv_buffer_read_uint(io, 1);
		r = mkv_alloc_samples(mkv, n + 1);
		if (0 != r) return r;

		size = 0; // first one
		for (i = 0; i < n; i++)
		{
			// The first size in the lace is unsigned as in EBML
			// The others use a range shifting to get a sign on each value
			lacing = 0 == i ? mkv_buffer_read_size(io) : mkv_buffer_read_signed_size(io);
			size += lacing; // diff with previous size
			mkv->samples[mkv->count + i].bytes = (uint32_t)size;
			total += size; // remain
		}

		pos2 = mkv_buffer_tell(io);
		if (total + (int64_t)(pos2 - pos) > bytes)
		{
			assert(0);
			return -1;
		}

		// last-frame(n+1)
		mkv->samples[mkv->count + n].bytes = (uint32_t)(bytes - total - (pos2 - pos));

		mkv->samples[mkv->count + 0].flags = f;
		mkv->samples[mkv->count + 0].track = tid;
		mkv->samples[mkv->count + 0].pts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].dts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].offset = mkv_buffer_tell(io);
		for (i = 1; i < n + 1; i++)
		{
			//mkv->samples[mkv->count + i].flags = mkv->samples[mkv->count + 0].flags;
			mkv->samples[mkv->count + i].track = mkv->samples[mkv->count + 0].track;
			mkv->samples[mkv->count + i].pts = mkv->samples[mkv->count + i - 1].pts + duration;
			mkv->samples[mkv->count + i].dts = mkv->samples[mkv->count + i - 1].dts + duration;
			mkv->samples[mkv->count + i].offset = mkv->samples[mkv->count + i - 1].offset + mkv->samples[mkv->count + i - 1].bytes;
		}

		mkv->count += n + 1;
		break;

	default: // no lacing
		// frame
		n = 0;
		r = mkv_alloc_samples(mkv, 1);
		if (0 != r) return r;

		mkv->samples[mkv->count + 0].flags = f;
		mkv->samples[mkv->count + 0].track = tid;
		mkv->samples[mkv->count + 0].pts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].dts = cluster->timestamp + timestamp;
		mkv->samples[mkv->count + 0].bytes = (uint32_t)(bytes - 4);
		mkv->samples[mkv->count + 0].offset = mkv_buffer_tell(io);
		mkv->count += 1;
		break;
	}

	if (f & MKV_FLAGS_KEYFRAME)
	{
		assert(mkv->count >= n + 1);
		mkv_add_rap(mkv, mkv->count - n - 1);
	}

	return 0;
}

int mkv_cluster_simple_block_write(struct mkv_t* mkv, struct mkv_sample_t* sample, struct mkv_ioutil_t* io)
{
	uint8_t flags;
	(void)mkv;
	assert(sample->track > 0 && sample->track < 8);

	flags = sample->flags & MKV_FLAGS_KEYFRAME ? 0x80 : 0;
	flags |= sample->flags & MKV_FLAGS_INVISIBLE ? 0x08 : 0;
	flags |= sample->flags & MKV_FLAGS_DISCARDABLE ? 0x01 : 0;

	// Segment/Cluster/SimpleBlock
	mkv_buffer_write_master(io, 0xA3, 4 + sample->bytes, 0);
	mkv_buffer_w8(io, (uint8_t)sample->track|0x80); // Track Number
	mkv_buffer_w16(io, (uint16_t)sample->pts); // Timestamp (relative to Cluster timestamp, signed int16)
	mkv_buffer_w8(io, flags); // SimpleBlock Header Flags

	// no lacing

	mkv_buffer_write(io, sample->data, sample->bytes);
	return 0;
}
