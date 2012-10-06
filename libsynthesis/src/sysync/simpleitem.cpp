/*
 *  File:         SimpleItem.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSimpleItem
 *    Simple item, no internal structure but just a string
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-06-18 : luz : created
 *
 */

// includes
#include "prefix_file.h"
#include "sysync.h"
#include "simpleitem.h"

#ifdef CHECKSUM_CHANGELOG
#include "sysync_crc16.h"
#endif


using namespace sysync;


/*
 * Implementation of TSimpleItem
 */


TSimpleItem::TSimpleItem(TSyncItemType *aItemTypeP) :
  TSyncItem(aItemTypeP)
{
  // save link to type
  fItemTypeP = aItemTypeP;
} // TSimpleItem::TSimpleItem


TSimpleItem::~TSimpleItem()
{
} // TSimpleItem::~TSimpleItem


#ifdef CHECKSUM_CHANGELOG

// changelog support: calculate CRC over contents
uInt16 TSimpleItem::getDataCRC(uInt16 crc, bool aEQRelevantOnly)
{
  // CRC of contents string (no change if nothing in it)
  return sysync_crc16_block(fContents.c_str(),fContents.size(),crc);
} // TSimpleItem::getDataCRC

#endif


// test if comparable (at least for equality)
bool TSimpleItem::comparable(TSyncItem &aItem)
{
  // test if comparable: only SimpleItems can be compared
  return aItem.getTypeID() == getTypeID();
} // TSimpleItem::comparable


// replace data contents from specified item
// - aAvailable only: only replace contents actually available in aItem, leave rest untouched
// - aDetectCutOffs: handle case where aItem could have somhow cut-off data and prevent replacing
//   complete data with cut-off version (e.g. mobiles like T39m with limited name string capacity)
bool TSimpleItem::replaceDataFrom(TSyncItem &aItem, bool aAvailableOnly, bool aDetectCutoffs, bool aAssignedOnly, bool aTransferUnassigned)
{
  // check type
  if (!aItem.isBasedOn(ity_simple)) return false;
  // ok, same type, copy data
  fContents=static_cast<TSimpleItem *>(&aItem)->fContents;
  return true;
} // TSimpleItem::replaceDataFrom



// compare function, returns 0 if equal, 1 if this > aItem, -1 if this < aItem
sInt16 TSimpleItem::compareWith(
  TSyncItem &aItem,
  TEqualityMode aEqMode,
  TLocalEngineDS *aDatastoreP
  #ifdef SYDEBUG
  ,bool aDebugShow
  #endif
)
{
  // we should test for comparable() before!
  if (!comparable(aItem)) return SYSYNC_NOT_COMPARABLE;
  TSimpleItem *simpleitemP = ((TSimpleItem *)&aItem);
  // compare (equality only, mod date/version is unknown)
  if (fContents == simpleitemP->fContents) return 0;
  else return SYSYNC_NOT_COMPARABLE;
} // TSimpleItem::compareWith


/* end of TSimpleItem implementation */

// eof
