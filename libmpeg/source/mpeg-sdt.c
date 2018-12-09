// ETSI EN 300 468 V1.15.1 (2016-03)
// Digital Video Broadcasting (DVB); Specification for Service Information (SI) in DVB systems
// 5.2.3 Service Description Table (SDT) (p27)

#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <string.h>
#include <assert.h>

size_t sdt_read(struct pat_t *pat, const uint8_t* data, size_t bytes)
{
    struct pmt_t* pmt;
    uint16_t sid;
    uint32_t i, k, n;
    uint8_t tagid, taglen, tagn1, tagn2;
    
    uint32_t table_id = data[0];
//    uint32_t section_syntax_indicator = (data[1] >> 7) & 0x01;
//    uint32_t zero = (data[1] >> 6) & 0x01;
//    uint32_t reserved = (data[1] >> 4) & 0x03;
    uint32_t section_length = ((data[1] & 0x0F) << 8) | data[2];
//    uint32_t transport_stream_id = (data[3] << 8) | data[4];
//    uint32_t reserved2 = (data[5] >> 6) & 0x03;
//    uint32_t version_number = (data[5] >> 1) & 0x1F;
//    uint32_t current_next_indicator = data[5] & 0x01;
//    uint32_t sector_number = data[6];
//    uint32_t last_sector_number = data[7];
//    uint32_t original_network_id = (data[8] << 8) | data[9];
//    uint32_t reserved4 = data[10];
    
//    printf("SDT: %0x %0x %0x %0x %0x %0x %0x %0x, %0x, %0x, %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6],(unsigned int)data[7],(unsigned int)data[8],(unsigned int)data[9],(unsigned int)data[10]);
    
    if(PAT_TID_SDT != table_id)
        return 0;
    
    // TODO: version_number change, reload SDT
    
    assert(bytes >= section_length + 3); // PMT = section_length + 3
    for (i = 11; i + 5 <= section_length + 3 - 4/*CRC32*/; i += 5 + n) // 9: follow section_length item
    {
        n = ((data[i+3] & 0x0F) << 8) | data[i+4];
        sid = (data[i] << 8) | data[i+1];
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
    assert(0 == mpeg_crc32(0xffffffff, data, section_length+3));
    return 0;
}
