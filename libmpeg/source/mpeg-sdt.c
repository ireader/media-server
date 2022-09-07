// ETSI EN 300 468 V1.15.1 (2016-03)
// Digital Video Broadcasting (DVB); Specification for Service Information (SI) in DVB systems
// 5.2.3 Service Description Table (SDT) (p27)

#include "mpeg-ts-proto.h"
#include "mpeg-element-descriptor.h"
#include "mpeg-util.h"
#include <string.h>
#include <assert.h>

#define SERVICE_ENCODER "encoder"

/*
service_description_section(){ 
    table_id 8 uimsbf
    section_syntax_indicator 1 bslbf
    reserved_future_use 1 bslbf
    reserved 2 bslbf
    section_length 12 uimsbf
    transport_stream_id 16 uimsbf
    reserved 2 bslbf
    version_number 5 uimsbf
    current_next_indicator 1 bslbf
    section_number 8 uimsbf
    last_section_number 8 uimsbf
    original_network_id 16 uimsbf
    reserved_future_use 8 bslbf
    for (i=0;i<N;i++){ 
        service_id 16 uimsbf
        reserved_future_use 6 bslbf
        EIT_schedule_flag 1 bslbf
        EIT_present_following_flag 1 bslbf
        running_status 3 uimsbf
        free_CA_mode 1 bslbf
        descriptors_loop_length 12 uimsbf
        for (j=0;j<N;j++){ 
            descriptor() 
        } 
    } 
 CRC_32 32 rpchof
}
*/
size_t sdt_read(struct pat_t *pat, const uint8_t* data, size_t bytes)
{
    struct pmt_t* pmt;
    uint16_t sid;
    uint32_t i, k, n, section_length;
    uint8_t tagid, taglen, tagn1, tagn2;
    
    if (bytes < 11)
        return 0;
//    printf("SDT: %0x %0x %0x %0x %0x %0x %0x %0x, %0x, %0x, %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6],(unsigned int)data[7],(unsigned int)data[8],(unsigned int)data[9],(unsigned int)data[10]);
//    uint32_t table_id = data[0];
//    uint32_t section_syntax_indicator = (data[1] >> 7) & 0x01;
//    uint32_t zero = (data[1] >> 6) & 0x01;
//    uint32_t reserved = (data[1] >> 4) & 0x03;
    section_length = ((data[1] & 0x0F) << 8) | data[2];
//    uint32_t transport_stream_id = (data[3] << 8) | data[4];
//    uint32_t reserved2 = (data[5] >> 6) & 0x03;
//    uint32_t version_number = (data[5] >> 1) & 0x1F;
//    uint32_t current_next_indicator = data[5] & 0x01;
//    uint32_t sector_number = data[6];
//    uint32_t last_sector_number = data[7];
//    uint32_t original_network_id = (data[8] << 8) | data[9];
//    uint32_t reserved4 = data[10];
    
    if(PAT_TID_SDT != data[0] || section_length + 3 > bytes)
        return 0;
    
    // TODO: version_number change, reload SDT
    
    assert(bytes >= section_length + 3); // PMT = section_length + 3
    for (i = 11; i + 5 <= section_length + 3 - 4/*CRC32*/ && section_length + 3 <= bytes; i += 5 + n) // 9: follow section_length item
    {
        sid = (data[i] << 8) | data[i + 1];
        n = ((data[i+3] & 0x0F) << 8) | data[i+4];
        // skip reserved/EIT data[i+2]
        if(i + 5 + n > section_length + 3 - 4)
            continue;
        
        pmt = pat_find(pat, sid);
        if(NULL == pmt)
            continue; // pmt not found
        
        for(k = i + 5; k + 2 <= i + 5 + n; k += 2 + taglen)
        {
            // 6.1 Descriptor identification and location
            tagid = data[k];
            taglen = data[k + 1];
            
            // 6.2.33 Service descriptor (p77)
            if(0x48 != tagid || k + taglen > i + 5 + n)
                continue;
            
            //service_type = data[k + 2];
            tagn1 = data[k + 3];
            if(tagn1 >= sizeof(pmt->provider) || k + 3 + tagn1 > i + 5 + n)
                continue;
            memcpy(pmt->provider, &data[k+4], tagn1);
            pmt->provider[tagn1] = 0;
            tagn2 = data[k + 4 + tagn1];
            if(tagn2 >= sizeof(pmt->name) || k + 5 + tagn1 + tagn2 > i + 5 + n)
                continue;
            memcpy(pmt->name, &data[k+5+tagn1], tagn2);
            pmt->name[tagn2] = 0;
        }
    }
    
    //assert(j+4 == bytes);
    //crc = (data[j] << 24) | (data[j+1] << 16) | (data[j+2] << 8) | data[j+3];
//    assert(0 == mpeg_crc32(0xffffffff, data, section_length+3));
    return section_length + 3;
}

size_t sdt_write(const struct pat_t* pat, uint8_t* data)
{
    size_t i, j;
    size_t len, s1, n1, v1;
    uint32_t crc = 0;

    n1 = strlen(SERVICE_ENCODER);
    v1 = strlen(SERVICE_NAME);
    s1 = 3 /*tag*/ + 1 + n1 + 1 + v1;
    len = 3 /*nid*/ + s1 + 5 /*service head*/ + 5 + 4; // 5 bytes remain header and 4 bytes crc32

    // shall not exceed 1021 (0x3FD).
    assert(len <= 1021);
    assert(len <= TS_PACKET_SIZE - 7);

    data[0] = PAT_TID_SDT;	// service_description_section

    // section_syntax_indicator = '1'
    // '0'
    // reserved '11'
    nbo_w16(data + 1, (uint16_t)(0xf000 | len));

    // transport_stream_id
    nbo_w16(data + 3, (uint16_t)pat->tsid);

    // reserved '11'
    // version_number 'xxxxx'
    // current_next_indicator '1'
    data[5] = (uint8_t)(0xC1 | (pat->ver << 1));

    // section_number/last_section_number
    data[6] = 0x00;
    data[7] = 0x00;

    // original_network_id
    nbo_w16(data + 8, (uint16_t)pat->tsid);
    data[10] = 0xFF; // reserved

    j = 11;
    // only one
    for (i = 0; i < 1; i++)
    {
        nbo_w16(data + j, (uint16_t)pat->tsid);
        data[j + 2] = 0xfc | 0x00; // no EIT

        assert(n1 < 255 && v1 < 255 && len < 255);
        nbo_w16(data + j + 3, (uint16_t)(0x8000 | s1));

        data[j + 5] = 0x48; // tag id
        data[j + 6] = (uint8_t)(3 + n1 + v1); // tag len
        data[j + 7] = 1; // service type
        data[j + 8] = (uint8_t)n1;
        memcpy(data + j + 9, SERVICE_NAME, n1);
        data[j + 9 + n1] = (uint8_t)v1;
        memcpy(data + j + 10 + n1, SERVICE_NAME, v1);
        j += 10 + v1 + n1;
    }

    // crc32
    crc = mpeg_crc32(0xffffffff, data, (uint32_t)j);
    data[j + 3] = (uint8_t)((crc >> 24) & 0xFF);
    data[j + 2] = (uint8_t)((crc >> 16) & 0xFF);
    data[j + 1] = (uint8_t)((crc >> 8) & 0xFF);
    data[j + 0] = (uint8_t)(crc & 0xFF);
    return j + 4;
}
