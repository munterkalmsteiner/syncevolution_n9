/*
 *  File:         SimpleItem.h
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

#ifndef SimpleItem_H
#define SimpleItem_H

// includes
#include "syncitem.h"
#include "sysync.h"


namespace sysync {

const uInt16 ity_simple = 1; // must be unique

class TSimpleItem: public TSyncItem
{
  typedef TSyncItem inherited;
public:
  TSimpleItem(TSyncItemType *aItemTypeP);
  virtual ~TSimpleItem();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_simple; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_simple ? true : TSyncItem::isBasedOn(aItemTypeID); };
  // assignment (IDs and contents)
  virtual TSyncItem& operator=(TSyncItem &aSyncItem) { return TSyncItem::operator=(aSyncItem); };
  // access to simple item contents
  const char *getContents(void) { return fContents.c_str(); };
  void setContents(const char *aContents) { fContents = aContents; };
  // compare abilities
  virtual bool comparable(TSyncItem &aItem);
  virtual bool sortable(TSyncItem &aItem) { return false; }
  // clear item data
  virtual void cleardata(void) { fContents.erase(); };
  // - changelog support
  #ifdef CHECKSUM_CHANGELOG
  virtual uInt16 getDataCRC(uInt16 crc=0, bool aEQRelevantOnly=false);
  #endif
  // replace data contents from specified item
  // - aAvailable only: only replace contents actually available in aItem, leave rest untouched
  // - aDetectCutOffs: handle case where aItem could have somhow cut-off data and prevent replacing
  //   complete data with cut-off version (e.g. mobiles like T39m with limited name string capacity)
  virtual bool replaceDataFrom(TSyncItem &aItem, bool aAvailableOnly=false, bool aDetectCutoffs=false, bool aAssignedOnly=false, bool aTransferUnassigned=false);
protected:
  // compare function, returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  virtual sInt16 compareWith(
    TSyncItem &aItem,
    TEqualityMode aEqMode,
    TLocalEngineDS *aDatastoreP
    #ifdef SYDEBUG
    ,bool aDebugShow=false
    #endif
  );
  // associated type item
  TSyncItemType *fItemTypeP;
  // contents: simple string
  string fContents;
}; // TSimpleItem

} // namespace sysync

#endif  // SimpleItem_H

// eof
