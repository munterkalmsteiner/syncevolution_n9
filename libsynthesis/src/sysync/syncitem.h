/*
 *  File:         SyncItem.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncItem
 *    Abstract Base class of all data containing items.
 *    TSyncItems always correspond to a TSyncItemType
 *    which holds type information and conversion
 *    features.
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-06-18 : luz : created
 *
 */

#ifndef SyncItem_H
#define SyncItem_H

// includes
#include "syncitemtype.h"


namespace sysync {


// default command size
#define DEFAULTITEMSIZE 50;


// equality relevance / mode
typedef enum {    // RELEVANCE                        COMPARE MODE
  eqm_none,       // not relevant at all (*)          compares ALL fields, even not-relevant ones
  eqm_scripted,   // relevant, handled in script      n/a
  eqm_conflict,   // relevant for conflicts only      compares all somehow relevant fields for conflict comparison
  eqm_slowsync,   // relevant for slow sync           compares only fields relevant for slow sync
  eqm_always,     // always relevant (e.g firsttime)  compares only always-relevant fields (first time slowsync)
  eqm_nocompare,  // n/a                              prevent equality test in compareWith totally
  numEQmodes
} TEqualityMode;
// (*) Note: eqm_none fields will be silently merged in mergeWith(), that is, if no other
//     fields are merged, mergeWith will report "nothing merged" even if eqm_none field might
//     be modified
//     Fields with eqm_none will also not be included in CRC calculations for detecting changes.

// Names for TEqualityMode when used as field relevance
extern const char * const compareRelevanceNames[numEQmodes];
  // Names for TEqualityMode when used as comparison mode
extern const char * const comparisonModeNames[numEQmodes];


// merge options
#define mem_none          -1   // do not merge at all
#define mem_fillempty     -2   // fill empty fields of winning item with loosig item's field contents
#define mem_addunassigned -3   // add fields of loosing item to winning item if it has this field not yet assigned (not just empty)
#define mem_concat         0   // simply concatenate field contents
// NOTE: any positive char constant means intelligent merge using given char as separator

// forward
class TLocalEngineDS;
class TSyncAppBase;

class TSyncItem
{
public:
  TSyncItem(TSyncItemType *aItemType=NULL);
  virtual ~TSyncItem();
  // access to type
  virtual uInt16 getTypeID(void) const { return ity_syncitem; };
  virtual bool isBasedOn(uInt16 aItemTypeID) const { return aItemTypeID==ity_syncitem; };
  // get session pointer
  TSyncSession *getSession(void) { return fSyncItemTypeP ? fSyncItemTypeP->getSession() : NULL; };
  // get session zones pointer
  GZones *getSessionZones(void);
  // assignment (IDs and contents)
  virtual TSyncItem& operator=(TSyncItem &aSyncItem);
  // access to IDs
  // - remote ID
  const char *getRemoteID(void) { return fRemoteID.c_str(); };
  void setRemoteID(const char *aRemoteID) { fRemoteID = aRemoteID; };
  bool hasRemoteID(void) { return !fRemoteID.empty(); };
  void clearRemoteID(void) { fRemoteID.erase(); };
  // - local ID
  const char *getLocalID(void) { return fLocalID.c_str(); };
  void setLocalID(const char *aLocalID) { fLocalID = aLocalID; };
  bool hasLocalID(void) { return !fLocalID.empty(); };
  void clearLocalID(void) { fLocalID.erase(); };
  virtual void updateLocalIDDependencies(void) { /* nop here */ }
  // - changelog support
  #ifdef CHECKSUM_CHANGELOG
  virtual uInt16 getDataCRC(uInt16 crc=0, bool /* aEQRelevantOnly */=false) { return crc; /* always empty */ };
  #endif
  // access to operation
  TSyncOperation getSyncOp(void) { return fSyncOp; };
  void setSyncOp(TSyncOperation aSyncOp) { fSyncOp=aSyncOp; };
  // access to visibility
  // object filtering
  #ifdef OBJECT_FILTERING
  // New style generic filtering
  // - check if item passes filter and probably apply some modifications to it
  virtual bool postFetchFiltering(TLocalEngineDS *aDatastoreP) { return true; } // without filters, just pass everything
  // Old style filter expressions
  // - test if item passes filter
  virtual bool testFilter(const char *aFilterString)
    { return (!aFilterString || *aFilterString==0); }; // without real test, only empty filterstring pass
  // - make item pass filter
  virtual bool makePassFilter(const char *aFilterString)
    { return (!aFilterString || *aFilterString==0); }; // without real implementation, works only with no filter string
  #endif
  // compare abilities
  // - test if comparable at all (correct types)
  virtual bool comparable(TSyncItem & /* aItem */) { return false; };
  // - test if comparison can find out newer (greater) item
  //   NOTE: this should reflect the actually possible sorting state
  //   related to current contents of the item (for example, a vCard
  //   that COULD be sorted if it HAD a REV date may not return true
  //   if the actual item does not have a REV property included.
  virtual bool sortable(TSyncItem & /* aItem */) { return false; };
  // clear item data (means not only empty values, but NO VALUES assigned)
  virtual void cleardata(void) {};
  // replace data contents from specified item (returns false if not possible)
  // - aAvailable only: only replace contents actually available in aItem, leave rest untouched
  // - aDetectCutOffs: handle case where aItem could have somhow cut-off data and prevent replacing
  //   complete data with cut-off version (e.g. mobiles like T39m with limited name string capacity)
  virtual bool replaceDataFrom(TSyncItem & /* aItem */, bool /* aAvailableOnly */=false, bool /* aDetectCutoffs */=false, bool /* aAssignedOnly */=false, bool /* aTransferUnassigned */=false) { return true; }; // no data -> nop
  // check item before processing it
  virtual bool checkItem(TLocalEngineDS * /* aDatastoreP */) { return true; }; // default is: ok
  // merge this item with specified item.
  // Notes:
  // - specified item is treated as loosing item, this item is winning item
  // - also updates other item to make sure it is equal to the winning after the merge
  // sets (but does not reset) change status of this and other item.
  // Note that changes of non-relevant fields are not reported here.
  virtual void mergeWith(TSyncItem & /* aItem */, bool &aChangedThis, bool &aChangedOther, TLocalEngineDS * /* aDatastoreP */) { aChangedThis=false; aChangedOther=false; }; // nop by default
  // remote and local ID
  string fRemoteID; // ID in remote party (if this is a server: LUID, GUID otherwise)
  string fLocalID;  // ID in this party (if this is a server: GUID, LUID otherwise)
  // compare: returns 0 if equal, 1 if this > aItem, -1 if this < aItem
  // SYSYNC_NOT_COMPARABLE if not equal and no ordering known
  virtual sInt16 compareWith(
    TSyncItem & /* aItem */,
    TEqualityMode /* aEqMode */,
    TLocalEngineDS * /* aDatastoreP */
    #ifdef SYDEBUG
    ,bool /* aDebugShow */=false
    #endif
  ) { return SYSYNC_NOT_COMPARABLE; };
  #ifdef SYDEBUG
  // show item contents for debug
  virtual void debugShowItem(uInt32 aDbgMask=DBG_DATA) { /* nop */ };
  // get debug channel
  TDebugLogger *getDbgLogger(void);
  uInt32 getDbgMask(void);
  #endif
  // - get session owner (dispatcher/clientbase)
  TSyncAppBase *getSyncAppBase(void);
protected:
  // operation to be performed with this item at its destination
  TSyncOperation fSyncOp;
  TSyncItemType *fSyncItemTypeP;
private:
  // cast pointer to same type, returns NULL if incompatible
  TSyncItem *castToSameTypeP(TSyncItem *aItemP) { return aItemP; } // all are compatible TSyncItem
}; // TSyncItem

} // namespace sysync

#endif  // SyncItem_H

// eof
