#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// Table 1 - List of Class Tags for Descriptors (p31)
/*
0x10 MP4_IOD_Tag
0x11 MP4_OD_Tag
*/

// 7.2.2.2 BaseDescriptor (p32)
/*
abstract aligned(8) expandable(2^28-1) class BaseDescriptor : bit(8) tag=0 {
// empty. To be filled by classes extending this class.
}
*/

// 7.2.6.2 ObjectDescriptorBase (p42)
/*
abstract class ObjectDescriptorBase extends BaseDescriptor : bit(8)
                    tag=[ObjectDescrTag..InitialObjectDescrTag] {
    // empty. To be filled by classes extending this class.
}
class ObjectDescriptor extends ObjectDescriptorBase : bit(8) tag=ObjectDescrTag {
    bit(10) ObjectDescriptorID;
    bit(1) URL_Flag;
    const bit(5) reserved=0b1111.1;
    if (URL_Flag) {
        bit(8) URLlength;
        bit(8) URLstring[URLlength];
    } else {
        ES_Descriptor esDescr[1 .. 255];
        OCI_Descriptor ociDescr[0 .. 255];
        IPMP_DescriptorPointer ipmpDescrPtr[0 .. 255];
        IPMP_Descriptor ipmpDescr [0 .. 255];
    }
    ExtensionDescriptor extDescr[0 .. 255];
}
*/

// 7.2.6.4 InitialObjectDescriptor (p44)
/*
class InitialObjectDescriptor extends ObjectDescriptorBase : bit(8)
                                    tag=InitialObjectDescrTag {
    bit(10) ObjectDescriptorID;
    bit(1) URL_Flag;
    bit(1) includeInlineProfileLevelFlag;
    const bit(4) reserved=0b1111;
    if (URL_Flag) {
        bit(8) URLlength;
        bit(8) URLstring[URLlength];
    } else {
        bit(8) ODProfileLevelIndication;
        bit(8) sceneProfileLevelIndication;
        bit(8) audioProfileLevelIndication;
        bit(8) visualProfileLevelIndication;
        bit(8) graphicsProfileLevelIndication;
        ES_Descriptor esDescr[1 .. 255];
        OCI_Descriptor ociDescr[0 .. 255];
        IPMP_DescriptorPointer ipmpDescrPtr[0 .. 255];
        IPMP_Descriptor ipmpDescr [0 .. 255];
        IPMP_ToolListDescriptor toolListDescr[0 .. 1];
    }
    ExtensionDescriptor extDescr[0 .. 255];
}
*/
size_t mov_write_iods(const struct mov_t* mov)
{
    size_t size = 12 /* full box */ + 12 /* InitialObjectDescriptor */;

    mov_buffer_w32(&mov->io, 24); /* size */
    mov_buffer_write(&mov->io, "iods", 4);
    mov_buffer_w32(&mov->io, 0); /* version & flags */
    
    mov_buffer_w8(&mov->io, 0x10); // ISO_MP4_IOD_Tag
    mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (7 >> 21)));
    mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (7 >> 14)));
    mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (7 >> 7)));
    mov_buffer_w8(&mov->io, (uint8_t)(0x7F & 7));

    mov_buffer_w16(&mov->io, 0x004f); // objectDescriptorId 1
    mov_buffer_w8(&mov->io, 0xff); // No OD capability required
    mov_buffer_w8(&mov->io, 0xff);
    mov_buffer_w8(&mov->io, 0xFF);
    mov_buffer_w8(&mov->io, 0xFF); // no visual capability required
    mov_buffer_w8(&mov->io, 0xff);

    return size;
}
