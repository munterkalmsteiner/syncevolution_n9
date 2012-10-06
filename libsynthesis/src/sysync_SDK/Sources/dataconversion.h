#ifndef DataConversion_H
#define DataConversion_H

#include "sync_declarations.h"

#ifdef __cplusplus

#include <string>
using namespace sysync;
namespace sysync {

/**
 * convert data from one format into another
 *
 * @param aSession         an active session handle
 * @param aFromTypeName    the original datatype name, like "vCard30"
 * @param aToTypeName      the generated datatype name
 * @retval aItemData       original data, replaced with data in new format
 * @return true for success
 */
bool DataConversion(SessionH aSession,
                    const char *aFromTypeName,
                    const char *aToTypeName,                    
                    std::string &aItemData);
} // namespace sysync

#endif /* __cplusplus */

#endif /* DataConversion_H */
