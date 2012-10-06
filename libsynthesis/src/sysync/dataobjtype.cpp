/*
 *  File:         DataObjType.cpp
 *
 *  Author:       Beat Forster (bfo@synthesis.ch)
 *
 *  TDataObjType
 *    base class for data object based items (EMAILOBJ, FILEOBJ, FOLDEROBJ)
 *    implemented for OMA DS V1.2
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *  2005-07-20 : bfo : created from TextItemType
 *
 */

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "dataobjtype.h"


using namespace sysync;

namespace sysync {

const char* BeginCDATA=          "<![CDATA[";
const char*   EndCDATA= "]]>";
const char* LowerCDATA= "]]]]>&gt;<![CDATA["; // only for the xml representation
const char* DoubleCEnd= "]]]]";


// Config
// ======
TTagMapDefinition::TTagMapDefinition(TConfigElement *aParentElementP, sInt16 aFid) :
  TConfigElement("lm",aParentElementP)
{
  fFid= aFid;  // save field ID
  clear();     // init others
} // TTagMapDefinition::TTagMapDefinition



void TTagMapDefinition::clear(void)
{
  // clear
  TCFG_CLEAR(fXmlTag);
  TCFG_CLEAR(fXmlAttr);
  fBoolType= false;
  fEmbedded= false;
  TCFG_CLEAR(fParent);
  #ifdef OBJECT_FILTERING
  TCFG_CLEAR(fFilterKeyword);
  #endif
} // TTagMapDefinition::clear



#ifdef CONFIGURABLE_TYPE_SUPPORT

// server config element parsing
bool TTagMapDefinition::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if      (strucmp(aElementName,"xmltag"       )==0) expectString(fXmlTag);        // the tag's name
  else if (strucmp(aElementName,"xmlattr"      )==0) expectString(fXmlAttr);       // and its (optional) attribute(s)
  else if (strucmp(aElementName,"booltype"     )==0) expectBool  (fBoolType);      // a boolean field
  else if (strucmp(aElementName,"embeddeditem" )==0) expectBool  (fEmbedded);      // an embedded item
  else if (strucmp(aElementName,"parent"       )==0) expectString(fParent);        // the parent's tag name
  #ifdef OBJECT_FILTERING
  else if (strucmp(aElementName,"filterkeyword")==0) expectString(fFilterKeyword); // the parent's tag name
  #endif

  // - none known here
  else return TConfigElement::localStartElement(aElementName,aAttributes,aLine);

  // ok
  return true;
} // TTagMapDefinition::localStartElement

#endif // CONFIGURABLE_TYPE_SUPPORT


TTagMapDefinition::~TTagMapDefinition() {
  // nop so far
} // TTagMapDefinition::~TTagMapDefinition



// -------------------------------------------------------------------------------------------
// XML based datatype config
TDataObjConfig::TDataObjConfig(const char* aName, TConfigElement *aParentElement) :
  inherited(aName,aParentElement)
{
  clear();
} // TDataObjConfig::TDataObjConfig


TDataObjConfig::~TDataObjConfig()
{
  // make sure everything is deleted (was missing long time and caused mem leaks!)
  clear();
} // TDataObjConfig::~TDataObjConfig


// init defaults
void TDataObjConfig::clear(void)
{
  // clear properties
  // - remove tag maps
  TTagMapList::iterator pos;
  for (pos= fTagMaps.begin();
       pos!=fTagMaps.end(); pos++) delete *pos;

  fTagMaps.clear();
  // clear inherited
  inherited::clear();
} // TDataObjConfig::clear


// create Sync Item Type of appropriate type from config
TSyncItemType *TDataObjConfig::newSyncItemType(TSyncSession *aSessionP, TSyncDataStore *aDatastoreP)
{
  if (!fFieldListP)
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TDataObjConfig::newSyncItemType: no fFieldListP","txit1")));
  return
    new TDataObjType(
      aSessionP,
      this,
      fTypeName.c_str(),
      fTypeVersion.c_str(),
      aDatastoreP,
      fFieldListP
    );
} // TDataObjConfig::newSyncItemType


#ifdef CONFIGURABLE_TYPE_SUPPORT

// config element parsing
bool TDataObjConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"tagmap")==0) {
    // <tagmap field="SUBJECT">
    const char* nam = getAttr(aAttributes,"field");
    if (!fFieldListP)
      return fail("'use' must be specfied before first <tagmap>");
    // search field
    // %%% add context here if we have any
    sInt16 fid = TConfigElement::getFieldIndex(nam,fFieldListP);
    if    (fid==VARIDX_UNDEFINED) return fail("'field' references unknown field '%s'",nam);

    // create new tagmap
    TTagMapDefinition *tagmapP= new TTagMapDefinition(this,fid);
    // - save in list
    fTagMaps.push_back(tagmapP);
    // - let element handle parsing
    expectChildParsing(*tagmapP);
    return true;
  } // if
  // - none known here
  if (inherited::localStartElement(aElementName,aAttributes,aLine))
    return true;
  // let the profile parse as if it was inside a <textprofile> or <mimeprofile>
  if (fProfileConfigP)
    return delegateParsingTo(fProfileConfigP,aElementName,aAttributes,aLine);
  // cannot parse
  return false;
} // TDataObjConfig::localStartElement


// resolve
void TDataObjConfig::localResolve(bool aLastPass)
{
  // nop
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TDataObjConfig::localResolve

#endif // CONFIGURABLE_TYPE_SUPPORT


#ifdef HARDCODED_TYPE_SUPPORT

TTagMapDefinition *TDataObjConfig::addTagMap( sInt16 aFid, const char* aXmlTag,
                                              bool aBoolType, bool aEmbedded,
                                              const char* aParent )
{
  // create new tagmap
  TTagMapDefinition *tagmapP = new TTagMapDefinition(this,aFid);

  // save the options
  TCFG_ASSIGN(tagmapP->fXmlTag,aXmlTag);
  tagmapP->fBoolType= aBoolType;
  tagmapP->fEmbedded= aEmbedded;
  tagmapP->fParent  = aParent;

  // save in list
  fTagMaps.push_back(tagmapP);
  // return pointer
  return tagmapP;
} // TDataObjConfig::addTagMap

#endif // HARDCODED_TYPE_SUPPORT



/*
 * Implementation of TDataObjType
 */
TDataObjType::TDataObjType(
  TSyncSession *aSessionP,
  TDataTypeConfig *aTypeCfgP, // type config
  const char *aCTType,
  const char *aVerCT,
  TSyncDataStore *aRelatedDatastoreP,
  TFieldListConfig *aFieldDefinitions // field definitions
) :
  TMultiFieldItemType(aSessionP,aTypeCfgP,aCTType,aVerCT,aRelatedDatastoreP,aFieldDefinitions),
  fProfileHandlerP(NULL)
{
  // save typed config pointer again
  fTypeCfgP = static_cast<TDataObjConfig *>(aTypeCfgP);
  // create the profile handler if there is one at all (a profile is optional)
  if (fTypeCfgP->fProfileConfigP) {
    // create the handler
    fProfileHandlerP = fTypeCfgP->fProfileConfigP->newProfileHandler(this);
    // set profile mode
    fProfileHandlerP->setProfileMode(fTypeCfgP->fProfileMode);
  }
} // TDataObjType::TDataObjType


TDataObjType::~TDataObjType()
{
  // handler not needed any more
  if (fProfileHandlerP)
    delete fProfileHandlerP;
} // TDataObjType::~TDataObjType



#ifdef OBJECT_FILTERING

// get field index of given filter expression identifier.
sInt16 TDataObjType::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // check if explicit field level identifier
  if (strucmp(aIdentifier,"F.",2)==0) {
    // explicit field identifier, skip property lookup
    aIdentifier+=2;
  }
  else {
    // search tagmaps for tagged fields
    TTagMapList::iterator pos;
    for (pos= fTypeCfgP->fTagMaps.begin();
         pos!=fTypeCfgP->fTagMaps.end(); pos++) {
      // first priority: compare with explicit filterkeyword, if any
      if (!TCFG_ISEMPTY((*pos)->fFilterKeyword)) {
        // compare with filterkeyword
        if (strucmp(TCFG_CSTR((*pos)->fFilterKeyword),aIdentifier)==0)
          return (*pos)->fFid;
      }
      else if (!TCFG_ISEMPTY((*pos)->fXmlTag)) {
        // compare with xml tag name itself
        if (strucmp(TCFG_CSTR((*pos)->fXmlTag),aIdentifier)==0)
          return (*pos)->fFid;
      } // if
    } // for
    // no matching tag found, let TTextProfileHandler search for its own filter identifiers
    if (fProfileHandlerP) {
      sInt16 fid = fProfileHandlerP->getFilterIdentifierFieldIndex(aIdentifier,aIndex);
      if (fid!=FID_NOT_SUPPORTED)
        return fid;
    }
    // no tagged field found, search underlying field list
  }
  // if no field ID found so far, look up in field list
  return TMultiFieldItemType::getFilterIdentifierFieldIndex(aIdentifier, aIndex);
} // TDataObjType::getFilterIdentifierFieldIndex

#endif // OBJECT_FILTERING



// helper to create same-typed instance via base class
TSyncItemType *TDataObjType::newCopyForSameType(
  TSyncSession *aSessionP,     // the session
  TSyncDataStore *aDatastoreP  // the datastore
)
{
  // create new itemtype of appropriate derived class type that can handle
  // this type
  MP_RETURN_NEW(TDataObjType,DBG_OBJINST,"TDataObjType",TDataObjType(
    aSessionP,
    fTypeConfigP,
    getTypeName(),
    getTypeVers(),
    aDatastoreP,
    fFieldDefinitionsP
  ));
} // TDataObjType::newCopyForSameType


// create new sync item of proper type and optimization for specified target
TSyncItem *TDataObjType::internalNewSyncItem(TSyncItemType *aTargetItemTypeP, TLocalEngineDS *aLocalDatastoreP)
{
  // All DataObjs are stored in MultiFieldItems
  if (!aTargetItemTypeP->isBasedOn(ity_multifield))
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TDataObjType::internalNewSyncItem with bad-typed target","txit3")));
  TMultiFieldItemType *targetitemtypeP =
    static_cast<TMultiFieldItemType *> (aTargetItemTypeP);
  return new TMultiFieldItem(this,targetitemtypeP);
} // TDataObjType::internalNewSyncItem


// fill in SyncML data (but leaves IDs empty)
bool TDataObjType::internalFillInData(
  TSyncItem *aSyncItemP,      // SyncItem to be filled with data
  SmlItemPtr_t aItemP,        // SyncML toolkit item Data to be converted into SyncItem (may be NULL if no data, in case of Delete or Map)
  TLocalEngineDS *aLocalDatastoreP, // local datastore
  TStatusCommand &aStatusCmd  // status command that might be modified in case of error
)
{
  // check type
  if (!aSyncItemP->isBasedOn(ity_multifield))
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TDataObjType::internalFillInData: incompatible item class","txit4")));
  TMultiFieldItem *itemP = static_cast<TMultiFieldItem *> (aSyncItemP);
  // process data if any
  if (aItemP->data) {
    // parse data
    stringSize sz;
    cAppCharP t = smlPCDataToCharP(aItemP->data,&sz);
    if (!parseData(t,sz,*itemP,aLocalDatastoreP)) {
      // format error
      aStatusCmd.setStatusCode(415); // Unsupported media type or format
      ADDDEBUGITEM(aStatusCmd,"Error parsing Text content");
      return false;
    }
  }
  else {
    // no data
    aStatusCmd.setStatusCode(412); // incomplete command
    ADDDEBUGITEM(aStatusCmd,"No data found in item");
    return false;
  }
  // ok, let ancestor process data as well
  return TMultiFieldItemType::internalFillInData(aSyncItemP,aItemP,aLocalDatastoreP,aStatusCmd);
} // TDataObjType::internalFillInData


// sets data and meta from SyncItem data, but leaves source & target untouched
bool TDataObjType::internalSetItemData(
  TSyncItem *aSyncItemP,  // the syncitem to be represented as SyncML
  SmlItemPtr_t aItem,     // item with NULL meta and NULL data
  TLocalEngineDS *aLocalDatastoreP // local datastore
)
{
  // check type
  if (!aSyncItemP->isBasedOn(ity_multifield))
    SYSYNC_THROW(TSyncException(DEBUGTEXT("TDataObjType::internalSetItemData: incompatible item class","txit4")));
  TMultiFieldItem *itemP = static_cast<TMultiFieldItem *> (aSyncItemP);
  // let ancestor prepare first
  if (!TMultiFieldItemType::internalSetItemData(aSyncItemP,aItem,aLocalDatastoreP)) return false;
  // generate data item
  string dataitem;
  generateData(aLocalDatastoreP,*itemP,dataitem);
  // put data item into opaque/cdata PCData (note that dataitem could be BINARY string, so we need to pass size!)
  aItem->data=newPCDataStringX((const uInt8 *)dataitem.c_str(),true,dataitem.size());
  // can't go wrong
  return true;
} // TDataObjType::internalSetItemData



static void AddXmlTag( string &aString, string aTag, bool newline= false )
{
               aString+= "<"  + aTag + ">";
  if (newline) aString+= "\n";
} // AddXmlTag


static void AddXmlTagEnd( string &aString, string aTag, bool newline= true )
{
               aString+= "</"  + aTag + ">";
  if (newline) aString+= "\n";
} // AddXmlTagEnd


static void AppendXml( string &aString, string aTag, const char* value, bool tagDone= false )
{
  if (*value!='\0') {
    if (!tagDone) AddXmlTag( aString, aTag );

                  aString+= value;
    AddXmlTagEnd( aString, aTag );
  } // if
} // AppendXml


static void AppendCDATA( string &aString, const char* aTag, const char* aValue )
{
  AddXmlTag( aString, aTag ); // must be shown according to OMA DS 1.2,
                              // even if the body is empty
  if (*aValue!='\0') {
    aString+= BeginCDATA; // only for the xml representation
    aString+= aValue;     // the body
    //%%% luz: not needed here as there is no enclosing CData any more (or this will be done when needed by the SML encoder!)
    //aString+= LowerCDATA; // only for the xml representation
    aString+=  '\n';
  } // if

  AddXmlTagEnd( aString, aTag );
} // AppendCDATA


string TDataObjType::getAttr( TMultiFieldItem &aItem, const char* aXmlTag, const char* aXmlAttr )
{
  TItemField* fieldP;
  TTagMapList::iterator pos;
  string v;

  // search for an element, which matches for both <aXmlTag> and <aXmlAttr>
  for (pos= fTypeCfgP->fTagMaps.begin();
       pos!=fTypeCfgP->fTagMaps.end(); pos++) {
     fieldP= aItem.getField( (*pos)->fFid ); // this is the element we're looking for
     if (strcmp( aXmlTag, TCFG_CSTR((*pos)->fXmlTag ))==0 &&
         strcmp( aXmlAttr,TCFG_CSTR((*pos)->fXmlAttr))==0) {
       fieldP->getAsString( v ); // this is the attribute we're looking for
       break;
     } // if
  } // for

  return v; // <v> is "", if no attribute found
} // TDataObjType::getAttr


// generate Data item (includes header and footer)
void TDataObjType::generateData(TLocalEngineDS *aDatastoreP, TMultiFieldItem &aItem, string &aString)
{
  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_GEN+DBG_HOT,("Generating:") );
  aItem.debugShowItem(DBG_GEN);
  #endif

  TItemField* fieldP;
  TItemField* fieldDocType; // the reference to the "DOCTYPE" (File/Folder/Email) field
  TTagMapList::iterator pos;

  string docT, v;
  string inParent;
  bool   tagOpened= false;

  //%%% luz: do not add extra CData here, this will be done when needed by the SML encoder!
  //aString= BeginCDATA; // start a CDATA sequence

      fieldDocType= aItem.getField( "DOCTYPE" );
  if (fieldDocType) {
      fieldDocType->getAsString( docT ); // main tag, straight forward implementation
    AddXmlTag         ( aString, docT, true );
  } // if

  // go thru the tag element list and get the matching element
  for (pos= fTypeCfgP->fTagMaps.begin();
       pos!=fTypeCfgP->fTagMaps.end(); pos++) {
         fieldP= aItem.getField( (*pos)->fFid ); // this is the element we're looking for
    if (!fieldP ||                               // not a valid field
         fieldP==fieldDocType) continue;         // has been done outside already, ignore it

    bool isArr= fieldP->isArray();
    sInt16      maxI= 1; // do it once w/o array
    if  (isArr) maxI= fieldP->arraySize();
    if         (maxI==0) continue; // it is an array w/o any items, skip it

    if ((*pos)->fEmbedded && fProfileHandlerP) {
      // set related datastore so handler can access session specific datastore state
      fProfileHandlerP->setRelatedDatastore(aDatastoreP);
      fProfileHandlerP->generateText(aItem, v);
      AppendCDATA( aString, TCFG_CSTR((*pos)->fXmlTag), v.c_str());
      continue; // it's done now for this element
    } // if

    // the standard case (not embedded): cover both cases: array AND no array
    sInt16 i;
    for (i= 0; i<maxI; i++) {
      TItemField* fieldAP= fieldP;
      if (isArr)  fieldAP= fieldP->getArrayField( i );

      if (fieldAP->isAssigned() &&
          TCFG_ISEMPTY((*pos)->fParent) &&
          !inParent.empty()) { AddXmlTagEnd( aString,inParent.c_str() ); inParent= ""; }

      // there is no native bool representation for the field lists
      // So do it with the help of an additional flag
      v= "";
      if   (fieldAP->isAssigned()) {
        if (fieldAP->isBasedOn( fty_blob ) &&
            fieldAP->hasProxy()) {
          #ifdef STREAMFIELD_SUPPORT
            string enc= getAttr( aItem, TCFG_CSTR((*pos)->fXmlTag), "enc" );

            // according OMA DS 1.2, the endcoding attribute "enc" must be
            // either not present (no encoding), "quoted-printable" or "base64".
            // The implementation SHOULD NOT use other enc attribute values.
            const char*  ptr = fieldAP->getCStr();
            size_t       size= fieldAP->getStreamSize();

            if (enc.empty()) {      // no encoding
              v.assign( ptr,size ); // => assign directly
            }
            else if (strucmp( enc.c_str(),"base64" )==0) {
              char*        bb= b64::encode( (uInt8*)ptr,size );
              v=           bb;   // maybe one copy operation can be avoided in future
              b64::free( bb ); // does never contain '\0' => can be treated as normal string
            }
            else if (strucmp( enc.c_str(),"quoted-printable")==0) {
              sysync::appendEncoded( (uInt8*)ptr,size, v, enc_quoted_printable,
                                     MIME_MAXLINESIZE,0,true,true );
            }
            else {
              v = "unknown_encoding: \"";
              v+=  enc;
              v+= "\"";
            }
          #endif
        }
        else if ((*pos)->fBoolType) {   // best fitting for integer and string
          int      ii= fieldAP->getAsInteger();
          switch ( ii ) {                // this is the integer - boolean mapping
            case -1: v= "false"; break; // a strange value for "false"
            case  0: v= "";      break; // 0 is the empty value (not assigned at parsing)
            case  1: v= "true";  break; // the 'normal' value for "true"
          } // switch
        } // if
        else fieldAP->getAsString( v );

        if (!v.empty()) {
          // avoid multiple identical elements (even if it's allowed in OMA DS 1.2)
          if  (!TCFG_ISEMPTY((*pos)->fParent)) {
            bool ok=                inParent.empty(); // either iParent==0 or strings are different
            if (!ok &&     strucmp( inParent.c_str(),TCFG_CSTR((*pos)->fParent) )!=0) {
              AddXmlTagEnd( aString,inParent.c_str() );   // switch off old tag first, if different
              ok= true;
            } // if

            if (ok) {            inParent= (*pos)->fParent;
              AddXmlTag( aString,inParent.c_str(), true );
            } // if
          } // if
        } // if
      } // if

      if (TCFG_ISEMPTY((*pos)->fXmlAttr)) { // w/o attributes
        if (tagOpened) aString+= ">";
        AppendXml    ( aString, (*pos)->fXmlTag, v.c_str(), tagOpened );
        tagOpened= false;
      }
      else if (!v.empty()) { // with XML attribute handling
        if (!tagOpened) {
          aString+= "<";
          aString+= TCFG_CSTR((*pos)->fXmlTag);
        } // if

        aString+=   " ";
        aString+=   TCFG_CSTR((*pos)->fXmlAttr);
        aString+=   "=\"" + v + "\"";
        tagOpened= true;
      } // if
    } // for

  } // for
  // Leave the block finally
  if (!inParent.empty()) AddXmlTagEnd( aString,inParent.c_str());

  if (fieldDocType) {
      fieldDocType->getAsString( docT ); // end of main tag, straight forward implementation
    AddXmlTagEnd      ( aString, docT );
  } // if

  //%%% luz: do not add extra CData here, this will be done when needed by the SML encoder!
  //aString+= EndCDATA; // terminate the sequence

  #ifdef SYDEBUG
    if (PDEBUGTEST(DBG_GEN)) {
      // note, do not use debugprintf because string is too long
      PDEBUGPRINTFX(DBG_GEN,("%s", "") );
      PDEBUGPRINTFX(DBG_GEN,("Successfully generated:") );
      PDEBUGPUTSX  (DBG_GEN+DBG_USERDATA, aString.c_str() );
      PDEBUGPRINTFX(DBG_GEN+DBG_USERDATA,("%s", "") );
    } // if
  #endif
} // TDataObjType::generateData


/*! Skip white spaces */
static void SkipWhiteSP( const char* &p )
{
  while (*p==0x0D || // CR
         *p==0x0A || // LF
         *p==0x09 || // HT
         *p==' ') p++; // skip leading CR/LF/Tabs and spaces
} // SkipWhiteSP


// simple straight forward letter recognition
static bool IsLetter( char ch )
{
  ch= toupper(ch);
  return ch>='A' && ch<='Z';
} // IsLetter


/*! Get the next tag and value of <p>
 *
 *  @param  <aTag>      may contain attributes, e.g. <body enc="base64">
 *  @param  <direction> +1 go down in the hierarchy
 *                       0 stay on the same level
 *                      -1 go up   in the hierarchy
 */
static bool NextTag( const char* &p, string &aTag, string &aAttr, const char* &aPos, int &aSize,
                                     int &level, int &direction )
{
  const char* BeginCom= "<!--";
  const char*   EndCom=  "-->";
  const char* q;

  // default values
  bool vDone= false;
  aAttr     = "";
  aPos      = "";
  aSize     = 0;
  direction = 0;

  SkipWhiteSP( p );
      q=  strstr( p,BeginCom ); // comment ?
  if (q && q==p) {
  }
  else {
    if (*p!='<') return false; // must start with a tag
    p++;
    if (*p=='/') { p++; direction= -1; }

    q=   strstr( p,">" ); if (!q) return false;
    aTag.assign( p, (unsigned int)( q-p ) );   // this is the tag

    int pos= aTag.find(" ");
    if (pos>=0) { // the tag contains some attributes
      aAttr= aTag.substr( pos+1 ); //  ==> get them
      aTag = aTag.substr( 0,pos ); // the real tag
    } // if

    p= q+1;
  } // if

  do {
    SkipWhiteSP( p );
    if (direction==-1) return true; // end tag reached

    if (*p!='<') break; // a value is coming now, not nested

    // is it a comment ?
         q=  strstr( p,BeginCom );
    if  (q && q==p) {
         q=  strstr( q,  EndCom ); if (!q) return false;
      p= q + strlen    ( EndCom ); continue; // comment is skipped
    } // if

         q=  strstr( p,"</" );
    if  (q && q==p) break; // an empty value

    // is it a CDATA section ?
         q=  strstr( p,BeginCDATA );
    if  (q && q==p) {
         p+= strlen(   BeginCDATA );
         q=  strstr( p,DoubleCEnd ); if (!q) return false;
      aPos = p;
      aSize= (unsigned int)( q-p );
      vDone= true; // don't do it later again

      p= q + strlen(   DoubleCEnd );

      q=     strstr( p,BeginCDATA ); if (!q) return false; // nested CDATA skipped
      p= q + strlen(   BeginCDATA );
      break;
    } // if

    if (!IsLetter( p[ 1 ] )) break; // no nested tag ?

    level++; direction= 1; // nested tags -> do handling outside
    return true;
  } while (true);

  SkipWhiteSP( p );
  q=   strstr( p, "</" ); if (!q) return false;

  if (!vDone) {
    aPos = p;
    aSize= (unsigned int)( q-p );
    vDone= true; // don't do it later again
  } // if

  p=  q+2;
  q=  strstr( p, aTag.c_str() ); if (!q || p!=q) return false;
  q+= aTag.size();
  q=  strstr( p,">" ); if (!q) return false;

  p= q+1;
  return true;
} // NextTag


// the structure for attributes with values
struct TAField {
  string name;
  string value;
}; // AField


/*
struct VVV {
  VVV() { printf( "hinein\n" ); }
 ~VVV() { printf( "hinaus '%s'\n", a.c_str() ); }
  string a;
};
*/


// parse Data item (includes header and footer)
bool TDataObjType::parseData(const char *aText, stringSize aSize, TMultiFieldItem &aItem, TLocalEngineDS *aDatastoreP)
{
  TItemField* fieldP;
  TTagMapList::iterator pos;
  string tag, attr, value, docType;
  const char* vPos;
  int         vSize;

  // Hierarchical tags
  typedef std::list<string> TStrList;
  TStrList tagHierarchy;
  TStrList::iterator px;

  TAField a;
  typedef std::list<TAField> TAList;
  TAList  aList;
  TAList::iterator ax;

  #ifdef SYDEBUG
  if (PDEBUGTEST(DBG_PARSE)) {
    // very detailed, show item being parsed
    PDEBUGPRINTFX(DBG_PARSE+DBG_HOT, ("Parsing: ") );
    PDEBUGPUTSX  (DBG_PARSE+DBG_USERDATA, aText);
  }
  #endif

  // Go thru all the tags and build temporary stack at <tList>
  int  level= 0;
  int  direction;
  const char*     p= aText;
  if     (strstr( p, BeginCDATA )==p) p+= strlen( BeginCDATA );
  while (NextTag( p, tag,attr, vPos,vSize, level,direction )) {
    // parse all attributes and put them into <aList>
    aList.clear();
    while (!attr.empty()) {
      int apos=     attr.find( "=\"" );
      if (apos<0) { attr= ""; break; }

      a.name = attr.substr( 0,apos );
      attr   = attr.substr( apos+2 );

          apos=     attr.find( "\"" );
      if (apos<0) { attr= ""; break; }

      a.value= attr.substr( 0,apos );
      attr   = attr.substr( apos+1 );
      while   (attr.find(" ")==0) attr= attr.substr( 1 );

      aList.push_back( a );
    } // while

    switch (direction) {
      case -1 :     px= tagHierarchy.end();         // are we going up in the hierarchy ?
                if (px!=tagHierarchy.begin())       // if the list is not empty ...
                        tagHierarchy.erase( --px ); // erase last element
                break;

      case  1 : tagHierarchy.push_back( tag );      // are we going down in the hierarchy ?
                if (level>1) break;

                       docType= tag;
                vPos = docType.c_str();
                vSize= docType.length();
                tag  = "DOCTYPE"; // assign the document type
                // go on for this special case

      default:
        // go thru the tag element list and get the matching element
        for (pos= fTypeCfgP->fTagMaps.begin();
             pos!=fTypeCfgP->fTagMaps.end(); pos++) {
          if        (strucmp( tag.c_str(),TCFG_CSTR((*pos)->fXmlTag) )==0) {
                  fieldP= aItem.getField( (*pos)->fFid ); // this is the element we're looking for
            if   (fieldP) {
              if (fieldP->isAssigned() &&         // -> allow multiple fields with the same name
                 !fieldP->isArray()) continue;    //    it is assigned already !

              // for embedded structures, the content shouldn't be copied
              // in all other cases, it can still be used as <value> string
              if (!(*pos)->fEmbedded) value.assign( vPos,vSize ); // this is the value of this tag

              bool  xAtt= !TCFG_ISEMPTY((*pos)->fXmlAttr);
              if (aList.size()==0) {
                if (xAtt) continue; // does not fit, both must be either empty
              }
              else {
                if (xAtt) {
                  value= "";

                  for (ax= aList.begin();
                       ax!=aList.end(); ax++) {
                    if (strucmp( (*ax).name.c_str(),TCFG_CSTR((*pos)->fXmlAttr) )==0) {
                      value=     (*ax).value.c_str(); break; // for attribute handling take as <value>.
                    } // if
                  } // for

                  if (value.empty()) continue;
                } // if
              } // if

              do {
                // there must be either no parent, or the parent must match
                if (         !TCFG_ISEMPTY((*pos)->fParent) &&
                     strucmp( TCFG_CSTR   ((*pos)->fParent),tagHierarchy.back().c_str() )!=0) break;

                if (fieldP->isBasedOn( fty_blob )) {
                  #ifdef STREAMFIELD_SUPPORT
                    string enc= getAttr( aItem, TCFG_CSTR((*pos)->fXmlTag), "enc" );

                    if (enc.empty()) {
                      // no encoding
                    }
                    else if (strucmp( enc.c_str(),"base64" )==0) {
                      uInt32 oLen;
                      uInt8*               bb= b64::decode( value.c_str(), 0, &oLen );
                      fieldP->setAsString( (const char*)bb, oLen ); // assign the value
                      b64::free        ( bb );
                      break; // already done now (blob assigned correctly
                    }
                    else if (strucmp( enc.c_str(),"quoted-printable")==0) {
                      string v;
                      sysync::appendDecoded( value.c_str(),value.length(), v, enc_quoted_printable );
                      fieldP->setAsString( v.c_str(), v.size() ); // assign the value
                      break; // already done now (blob assigned correctly
                    }
                    else {
                      value = "unknown_encoding: \"";
                      value+=  enc;
                      value+= "\"";
                    }
                  #endif
                } // if

                // we don't have a boolean type directly, so make a special conversion for it
                if ((*pos)->fBoolType) {            // assuming the field list item is integer for boolean
                  if      (strucmp( value.c_str(),"0"     )==0) break;       // as defined in OMA DS 1.2
                  else if (strucmp( value.c_str(),"false" )==0) value= "-1"; // special boolean treatement
                  else if (strucmp( value.c_str(),"true"  )==0) value=  "1";
                } // if

                // delegate parsing of embedded object
                if ((*pos)->fEmbedded && fProfileHandlerP) {
                  // set related datastore so handler can access session specific datastore state
                  fProfileHandlerP->setRelatedDatastore(aDatastoreP);
                  // vPos,vSize is not a copy, but a direct reference into <aText>
                  bool ok= fProfileHandlerP->parseText(vPos,vSize, aItem);
                  if (!ok) {
                    // format error
                    // aStatusCmd.setStatusCode(415); // Unsupported media type or format
                    // ADDDEBUGITEM(aStatusCmd,"Error parsing Text content");
                    return false;
                  } // if

                  break; // already done (at profile handler)
                } // if

                // it's ok now to fill in <value>
                if (fieldP->isArray()) fieldP->appendString( value.c_str() );
                else                   fieldP->setAsString ( value.c_str() ); // assign the value
              } while (false);
            } // if

            if (aList.size()==0) break;
          } // if
        } // for
    } // switch
  } // while

  #ifdef SYDEBUG
  PDEBUGPRINTFX(DBG_PARSE,("Successfully parsed: "));
  aItem.debugShowItem(DBG_DATA+DBG_PARSE);
  #endif

  return true;
} // TDataObjType::parseData


// generates SyncML-Devinf property list for type
SmlDevInfCTDataPropListPtr_t TDataObjType::newCTDataPropList(TTypeVariantDescriptor aVariantDescriptor)
{
  // no properties here
  return NULL;
} // TDataObjType::newCTDataPropList


// Filtering: add keywords and property names to filterCap
void TDataObjType::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor)
{
  // add keywords from tagmaps
  #ifdef OBJECT_FILTERING
  TTagMapList::iterator pos;
  for(pos=fTypeCfgP->fTagMaps.begin();pos!=fTypeCfgP->fTagMaps.end();pos++) {
    // first priority: compare with explicit filterkeyword, if any
    if (!TCFG_ISEMPTY((*pos)->fFilterKeyword)) {
      // has a filterkeyword, show it
      addPCDataStringToList(TCFG_CSTR((*pos)->fFilterKeyword), &aFilterKeywords);
    }
  }
  // let embedded profile add the keywords
  if (fProfileHandlerP) {
    fProfileHandlerP->addFilterCapPropsAndKeywords(aFilterKeywords, aFilterProps, aVariantDescriptor, this);
  }
  // let base class add own keywords/props
  inherited::addFilterCapPropsAndKeywords(aFilterKeywords, aFilterProps, aVariantDescriptor);
  #endif
} // TDataObjType::addFilterCapPropsAndKeywords


// intended for creating SyncItemTypes for remote databases from
// transmitted DevInf.
// SyncItemType MUST NOT take ownership of devinf structure passed
// (because multiple types might be created from a single CTCap entry)
bool TDataObjType::analyzeCTCap(SmlDevInfCTCapPtr_t aCTCapP)
{
  // just let parent handle
  return inherited::analyzeCTCap(aCTCapP);
} // TDataObjType::analyzeCTCap


/// @brief copy CTCap derived info from another SyncItemType
/// @return false if item not compatible
/// @note required to create remote type variants from ruleMatch type alternatives
bool TDataObjType::copyCTCapInfoFrom(TSyncItemType &aSourceItem)
{
  // just let parent handle
  return inherited::copyCTCapInfoFrom(aSourceItem);
} // TDataObjType::copyCTCapInfoFrom


} // namespace sysync

/* end of TDataObjType implementation */
// eof
