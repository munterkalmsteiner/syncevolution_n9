/* b64 encoding/decoding */

#ifndef SYSYNC_B64_H
#define SYSYNC_B64_H

#include "generic_types.h"

using namespace sysync;

namespace b64 {

// encode data to B64, returns allocated buffer
// does line breaks if maxLineLen!=0
char *encode (
  const uInt8 *instr, uInt32 len, uInt32 *outlenP=NULL,
  sInt16 maxLineLen=0, bool crLineEnd=false
);

// decode B64 string to data (len=0 calculates string length automatically)
uInt8 *decode(const char *instr, uInt32 len=0, uInt32 *outlenP=NULL);

// free memory allocated with encode or decode above
void free(void *mem);

}

#endif /* SYSYNC_B64_H */

/* eof */
