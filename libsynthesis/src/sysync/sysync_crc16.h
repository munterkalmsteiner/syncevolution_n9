/*
 *  sysync_crc16.h
 *    CRC 16 checksumming functions
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 */

/* compute 16 bit CCITT crc */

#include "generic_types.h"

namespace sysync {

/* add next byte to CRC */
uInt16 sysync_crc16(uInt16 crc,uInt8 b);

// add next block of bytes to CRC
uInt16 sysync_crc16_block(const void* dataP, uInt32 len, uInt16 crc);

} // namespace sysync

/* eof */
