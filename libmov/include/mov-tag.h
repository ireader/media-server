#ifndef _mov_tag_h_
#define _mov_tag_h_

// ISO/IEC 14496-1:2010(E)
// 7.2.2 Common data structures
// Table-1 List of Class Tags for Descriptors (p31)
enum {
	ISO_ObjectDescrTag			= 0x01,
	ISO_InitialObjectDescrTag	= 0x02,
	ISO_ESDescrTag				= 0x03,
	ISO_DecoderConfigDescrTag	= 0x04,
	ISO_DecSpecificInfoTag		= 0x05,
	ISO_SLConfigDescrTag		= 0x06,
	ISO_ContentIdentDescrTag	= 0x07,
	ISO_SupplContentIdentDescrTag = 0x08,
	ISO_IPI_DescrPointerTag		= 0x09,
	ISO_IPMP_DescrPointerTag	= 0x0A,
	ISO_IPMP_DescrTag			= 0x0B,
	ISO_QoS_DescrTag			= 0x0C,
	ISO_RegistrationDescrTag	= 0x0D,
	ISO_ES_ID_IncTag			= 0x0E,
	ISO_ES_ID_RefTag			= 0x0F,
	ISO_MP4_IOD_Tag				= 0x10,
	ISO_MP4_OD_Tag				= 0x11,
};

// ISO/IEC 14496-1:2010(E)
// 7.2.2.3 BaseCommand
// Table-2 List of Class Tags for Commands (p33)
enum {
	ISO_ObjectDescrUpdateTag	= 0x01,
	ISO_ObjectDescrRemoveTag	= 0x02,
	ISO_ES_DescrUpdateTag		= 0x03,
	ISO_ES_DescrRemoveTag		= 0x04,
	ISO_IPMP_DescrUpdateTag		= 0x05,
	ISO_IPMP_DescrRemoveTag		= 0x06,
	ISO_ES_DescrRemoveRefTag	= 0x07,
	ISO_ObjectDescrExecuteTag	= 0x08,
	ISO_User_Private			= 0xC0,
};

#endif /* !_mov_tag_h_ */
