// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.4.8 Program map table(p68)

#include "mpeg-ts-internal.h"
#include "mpeg-ts-opus.h"
#include "mpeg-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static struct pes_t* pmt_fetch(struct pmt_t* pmt, uint16_t pid)
{
    unsigned int i;
    for(i = 0; i < pmt->stream_count; i++)
    {
        if(pmt->streams[i].pid == pid)
            return &pmt->streams[i];
    }
    
    if(pmt->stream_count >= sizeof(pmt->streams) / sizeof(pmt->streams[0]))
    {
        assert(0);
        return NULL;
    }
    
    // new stream
    return &pmt->streams[pmt->stream_count++];
}

static int pmt_read_descriptor(struct pes_t* stream, const uint8_t* data, uint16_t bytes)
{
	uint8_t tag;
	uint8_t len;
	uint8_t channels;

	// Registration descriptor
	while (bytes > 2)
	{
		tag = data[0];
		len = data[1];
		if (len + 2 > bytes)
			return -1; // invalid len

		// ISO/IEC 13818-1:2018 (E) Table 2-45 ?Program and program element descriptors (p90)
		switch (tag)
		{
		case 0x05: // 2.6.8 Registration descriptor(p94)
			if (len >= 4 && 'O' == data[2] && 'p' == data[3] && 'u' == data[4] && 's' == data[5])
			{
				assert(PSI_STREAM_PRIVATE_DATA == stream->codecid);
				stream->codecid = PSI_STREAM_AUDIO_OPUS;
			}
			if (len >= 4 && 'A' == data[2] && 'V' == data[3] && '0' == data[4] && '1' == data[5])
			{
				// https://aomediacodec.github.io/av1-mpeg2-ts/
				// Constraints on AV1 streams in MPEG-2 TS
				assert(PSI_STREAM_PRIVATE_DATA == stream->codecid);
				stream->codecid = PSI_STREAM_AV1;
			}
			else if (len >= 4 && 'A' == data[2] && 'V' == data[3] && 'S' == data[4] && '3' == data[5])
			{
				stream->codecid = PSI_STREAM_VIDEO_AVS3;
			}
			break;

		case 0x7f: // DVB-Service Information: 6.1 Descriptor identification and location (p38)
			// 2.6.90 Extension descriptor
			if (len >= 2 && data[3] <= sizeof(opus_channel_map)/sizeof(opus_channel_map[0]) && PSI_STREAM_AUDIO_OPUS == stream->codecid && OPUS_EXTENSION_DESCRIPTOR_TAG == data[2]) // User defined (provisional Opus)
			{
				channels = data[3];
				stream->esinfo = (uint8_t*)calloc(1, sizeof(opus_default_extradata));
				if (stream->esinfo)
				{
					stream->esinfo_len = sizeof(opus_default_extradata);
					memcpy(stream->esinfo, opus_default_extradata, stream->esinfo_len);
					stream->esinfo[9] = channels ? channels : 2;
					stream->esinfo[18] = channels ? (channels > 2 ? 1 : 0) : /* Dual Mono */ 255;
					stream->esinfo[19] = opus_stream_cnt[channels];
					stream->esinfo[20] = opus_coupled_stream_cnt[channels];
					memcpy(stream->esinfo + 21, opus_channel_map[(channels ? channels : 2) - 1], channels ? channels : 2);
				}
			}
		}

		data += len + 2;
		bytes -= len + 2;
	}
	assert(0 == bytes);

	return 0;
}

static int pmt_write_descriptor(const struct pes_t* stream, uint8_t* data, int bytes)
{
	uint8_t* p;

	p = data;
	if (PSI_STREAM_AUDIO_OPUS == stream->codecid && bytes > 2 + 4 /*fourcc*/ + 4 /*DVI OPUS*/ )
	{
		*p++ = 0x05; // 2.6.8 Registration descriptor(p94)
		*p++ = 4;
		memcpy(p, "Opus", 4);
		p += 4;

		*p++ = 0x7f; // DVB-Service Information: 6.1 Descriptor identification and location (p38)
		*p++ = 2;
		*p++ = OPUS_EXTENSION_DESCRIPTOR_TAG;
		*p++ = stream->esinfo_len > 8 ? stream->esinfo[9] : 2 /*0xFF*/ ; // default 2-channels
	}

	return (int)(p - data);
}

size_t pmt_read(struct pmt_t *pmt, const uint8_t* data, size_t bytes)
{
    struct pes_t* stream;
    uint16_t pid, len;
	uint32_t i, section_length, program_number, version_number;
	uint32_t PCR_PID, program_info_length;

	if (bytes < 12)
		return 0; // invalid data length
//	printf("PMT: %0x %0x %0x %0x %0x %0x %0x %0x, %0x, %0x, %0x, %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6],(unsigned int)data[7],(unsigned int)data[8],(unsigned int)data[9],(unsigned int)data[10],(unsigned int)data[11]);
	assert(PAT_TID_PMS == data[0]);
	assert(1 == ((data[1] >> 7) & 0x01));
	section_length = ((data[1] & 0x0F) << 8) | data[2];
	program_number = (data[3] << 8) | data[4];
//	uint32_t reserved2 = (data[5] >> 6) & 0x03;
	version_number = (data[5] >> 1) & 0x1F;
//	uint32_t current_next_indicator = data[5] & 0x01;
	assert(0 == data[6]); // sector_number
	assert(0 == data[7]); // last_sector_number
//	uint32_t reserved3 = (data[8] >> 5) & 0x07;
	PCR_PID = ((data[8] & 0x1F) << 8) | data[9];
//	uint32_t reserved4 = (data[10] >> 4) & 0x0F;
	program_info_length = ((data[10] & 0x0F) << 8) | data[11];

	if (PAT_TID_PMS != data[0] || section_length + 3 < 12 + 4 /*crc32*/ || section_length + 3 > bytes
		|| program_info_length + 12 /*head*/ > section_length + 3 /*head*/ - 4 /*crc32*/ )
	{
		assert(0);
		return 0; // invalid data length
	}

	//if(pmt->ver != version_number)
	//	pmt_clear(pmt); // fix pmt.pes.pkt.data memory leak

	pmt->PCR_PID = PCR_PID;
	pmt->pn = program_number;
	//pmt->ver = version_number;
	pmt->pminfo_len = program_info_length;

	if(program_info_length > 2)
	{
		// descriptor(data + 12, program_info_length)
	}

PMT_VERSION_CHANGE:
	assert(bytes >= section_length + 3); // PMT = section_length + 3
    for (i = 12 + program_info_length; i + 5 <= section_length + 3 - 4/*CRC32*/ && section_length + 3 <= bytes; i += len + 5) // 9: follow section_length item
	{
        pid = ((data[i+1] & 0x1F) << 8) | data[i+2];
        len = ((data[i+3] & 0x0F) << 8) | data[i+4];
//        printf("PMT: pn: %0x, pid: %0x, codec: %0x, eslen: %d\n", (unsigned int)pmt->pn, (unsigned int)pid, (unsigned int)data[i], (unsigned int)len);

		if (i + len + 5 > section_length + 3 - 4/*CRC32*/)
			break; // mark error ?

        assert(pmt->stream_count <= sizeof(pmt->streams)/sizeof(pmt->streams[0]));
        stream = pmt_fetch(pmt, pid);
		if (NULL == stream)
		{
			if (pmt->ver != version_number)
			{
				pmt->ver = version_number; // once only
				pmt_clear(pmt);
				goto PMT_VERSION_CHANGE;
			}
			continue;
		}
        
        stream->pn = (uint16_t)pmt->pn;
        stream->pid = pid;
        stream->codecid = data[i];
		stream->esinfo_len = 0; // default nothing
		if (len > 2)
		{
			//descriptor(data + i + 5, len)
			pmt_read_descriptor(stream, data + i + 5, len);
		}
	}

	pmt->ver = version_number;
	//assert(j+4 == bytes);
	//crc = (data[j] << 24) | (data[j+1] << 16) | (data[j+2] << 8) | data[j+3];
//	assert(0 == mpeg_crc32(0xffffffff, data, section_length+3));
	return section_length + 3;
}

size_t pmt_write(const struct pmt_t *pmt, uint8_t *data)
{
	// 2.4.4.8 Program map table (p68)
	// Table 2-33

	uint32_t i = 0;
	uint32_t crc = 0;
	ptrdiff_t len = 0;
	uint8_t *p = NULL;

	data[0] = PAT_TID_PMS;	// program map table

	// skip section_length

	// program_number
	nbo_w16(data + 3, (uint16_t)pmt->pn);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[5] = (uint8_t)(0xC1 | (pmt->ver << 1));

	// section_number/last_section_number
	data[6] = 0x00;
	data[7] = 0x00;

	// reserved '111'
	// PCR_PID 13-bits 0x1FFF
	nbo_w16(data + 8, (uint16_t)(0xE000 | pmt->PCR_PID));

	// reserved '1111'
	// program_info_length 12-bits, the first two bits of which shall be '00'.
	assert(pmt->pminfo_len < 0x400);
	nbo_w16(data + 10, (uint16_t)(0xF000 | pmt->pminfo_len));
	if(pmt->pminfo_len > 0 && pmt->pminfo_len < 0x400)
	{
		// fill program info
		assert(pmt->pminfo);
		memcpy(data + 12, pmt->pminfo, pmt->pminfo_len);
	}

	// streams
	p = data + 12 + pmt->pminfo_len;
	for(i = 0; i < pmt->stream_count && p - data < 1021 - 4 - 5 - pmt->streams[i].esinfo_len; i++)
	{
		// stream_type
		*p = (uint8_t)(PSI_STREAM_AUDIO_OPUS == pmt->streams[i].codecid) ? PSI_STREAM_PRIVATE_DATA : pmt->streams[i].codecid;

		// reserved '111'
		// elementary_PID 13-bits
		nbo_w16(p + 1, 0xE000 | pmt->streams[i].pid);

		len = 0;
		// fill elementary stream info
		//if(PSI_STREAM_AUDIO_OPUS == pmt->streams[i].codecid || (pmt->streams[i].esinfo_len > 0 && pmt->streams[i].esinfo))
		{
			//assert(pmt->streams[i].esinfo);
			//memcpy(p, pmt->streams[i].esinfo, pmt->streams[i].esinfo_len);
			//p += pmt->streams[i].esinfo_len;
			len = pmt_write_descriptor(&pmt->streams[i], p + 5, 1021 - (int)(p + 5 - data));
		}

		// reserved '1111'
		// ES_info_lengt 12-bits
		nbo_w16(p + 3, 0xF000 | (uint16_t)len);
		p += 5 + len;
	}

	// section_length
	len = p + 4 - (data + 3); // 4 bytes crc32
	assert(len <= 1021); // shall not exceed 1021 (0x3FD).
	assert(len <= TS_PACKET_SIZE - 7);
	// section_syntax_indicator '1'
	// '0'
	// reserved '11'
	nbo_w16(data + 1, (uint16_t)(0xb000 | len)); 

	// crc32
	crc = mpeg_crc32(0xffffffff, data, (uint32_t)(p-data));
	//put32(p, crc);
	p[3] = (crc >> 24) & 0xFF;
	p[2] = (crc >> 16) & 0xFF;
	p[1] = (crc >> 8) & 0xFF;
	p[0] = crc & 0xFF;

	return (p - data) + 4; // total length
}

void pmt_clear(struct pmt_t* pmt)
{
	unsigned int i;
	struct pes_t* pes;

	for (i = 0; i < pmt->stream_count; i++)
	{
		pes = &pmt->streams[i];
		if (pes->pkt.data)
		{
			free(pes->pkt.data);
			pes->pkt.data = NULL;
		}
		pes->pkt.size = 0;

		if (pes->esinfo)
		{
			free(pes->esinfo);
			pes->esinfo = NULL;
		}
		pes->esinfo_len = 0;
	}
	pmt->stream_count = 0;

	if (pmt->pminfo)
	{
		free(pmt->pminfo);
		pmt->pminfo = NULL;
		
	}
	pmt->pminfo_len = 0; 
}
