/*
 *  File:         textprofile.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TTextProfile
 *    utility class to parse line-by-line type text including RFC822 emails
 *
 *  Copyright (c) 2002-2011 by Synthesis AG + plan44.ch
 *
 *  2005-07-26 : luz : extracted from textitemtype
 *
 */


// includes
#include "prefix_file.h"

#include "sysync.h"
#include "textprofile.h"


using namespace sysync;


// Config
// ======

#define X_LIMIT_HEADER_NAME "X-Sync-Message-Limit"

// line map config

TLineMapDefinition::TLineMapDefinition(TConfigElement *aParentElementP, sInt16 aFid) :
  TConfigElement("lm",aParentElementP)
{
  // save field ID
  fFid=aFid;
  // init others
  clear();
} // TLineMapDefinition::TLineMapDefinition



void TLineMapDefinition::clear(void)
{
  // clear
  // - default: all text
  fNumLines=0;
  fInHeader=false; // not restricted to header
  fAllowEmpty=false; // no empty ones
  // - no tagged header
  TCFG_CLEAR(fHeaderTag);
  // - no RFC822 specials
  fValueType=vt822_plain;
  fListSeparator=0;
  fMaxRepeat=1;
  fRepeatInc=1;
  #ifdef OBJECT_FILTERING
  // - no filterkeyword
  TCFG_CLEAR(fFilterKeyword);
  #endif
} // TLineMapDefinition::clear


#ifdef CONFIGURABLE_TYPE_SUPPORT

// Conversion names for RFC(2)822 parsing
const char * const RFC822ValueTypeNames[num822ValueTypes] = {
  "text",
  "date",
  "body",
  "rfc2047"
};



// server config element parsing
bool TLineMapDefinition::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"numlines")==0)
    expectInt16(fNumLines);
  else if (strucmp(aElementName,"inheader")==0)
    expectBool(fInHeader);
  else if (strucmp(aElementName,"allowempty")==0)
    expectBool(fAllowEmpty);
  else if (strucmp(aElementName,"headertag")==0)
    expectString(fHeaderTag);
  // filtering
  #ifdef OBJECT_FILTERING
  else if (strucmp(aElementName,"filterkeyword")==0)
    expectString(fFilterKeyword);
  #endif
  // RFC(2)822 parsing options
  else if (strucmp(aElementName,"valuetype")==0)
    expectEnum(sizeof(fValueType),&fValueType,RFC822ValueTypeNames,num822ValueTypes);
  else if (strucmp(aElementName,"list")==0) {
    // list spec
    // - separator
    const char *attr = getAttr(aAttributes,"separator");
    if (!attr)
      fail("list needs 'separator'");
    fListSeparator=*attr;
    // - max repetitions
    fMaxRepeat=1;
    attr = getAttr(aAttributes,"repeat");
    if (attr) {
      #ifdef ARRAYFIELD_SUPPORT
      if (strucmp(attr,"array")==0) fMaxRepeat=REP_ARRAY;
      else
      #endif
      if (!StrToShort(attr,fMaxRepeat))
        return !fail("expected number or 'array' in 'repeat'");
    }
    fRepeatInc=1;
    if (!getAttrShort(aAttributes,"increment",fRepeatInc,true))
      return !fail("number expected in 'increment'");
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TLineMapDefinition::localStartElement

#endif


TLineMapDefinition::~TLineMapDefinition()
{
  // nop so far
} // TLineMapDefinition::~TLineMapDefinition



// text-based datatype config

TTextProfileConfig::TTextProfileConfig(const char *aElementName, TConfigElement *aParentElementP) :
  TProfileConfig(aElementName,aParentElementP)
{
  clear();
} // TTextProfileConfig::TTextProfileConfig


TTextProfileConfig::~TTextProfileConfig()
{
  // make sure everything is deleted (was missing long time and caused mem leaks!)
  clear();
} // TTextProfileConfig::~TTextProfileConfig


// init defaults
void TTextProfileConfig::clear(void)
{
  // clear properties
  // - init
  fFieldListP=NULL;
  #ifdef EMAIL_FORMAT_SUPPORT
  fMIMEMail=false;
  fBodyMIMETypesFid=VARIDX_UNDEFINED;
  fSizeLimitField=VARIDX_UNDEFINED;
  fBodyCountFid=VARIDX_UNDEFINED;
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  // - no limit
  fMaxAttachments=29999; // enough
  // - fields not yet known
  fAttachmentCountFid=VARIDX_UNDEFINED;
  fAttachmentMIMETypesFid=VARIDX_UNDEFINED;
  fAttachmentContentsFid=VARIDX_UNDEFINED;
  fAttachmentSizesFid=VARIDX_UNDEFINED;
  fAttachmentNamesFid=VARIDX_UNDEFINED;
  #endif
  #endif
  // - remove line maps
  TLineMapList::iterator pos;
  for(pos=fLineMaps.begin();pos!=fLineMaps.end();pos++)
    delete *pos;
  fLineMaps.clear();
  // clear inherited
  inherited::clear();
} // TTextProfileConfig::clear


#ifdef CONFIGURABLE_TYPE_SUPPORT


// config element parsing
bool TTextProfileConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  /* %%% this must be enabled if we want to reference script vars
  #ifdef SCRIPT_SUPPORT
  // early resolve basic multifield scripts so we can refer to local vars
  if (!fScriptsResolved) %%% resolve resolvecontext;
  #endif
  */
  // checking the elements
  if (strucmp(aElementName,"linemap")==0) {
    // <linemap field="SUBJECT">
    const char* nam = getAttr(aAttributes,"field");
    if (!fFieldListP)
      return fail("'use' must be specified before first <linemap>");
    // search field
    // %%% add context here if we have any
    sInt16 fid = TConfigElement::getFieldIndex(nam,fFieldListP);
    if (fid==VARIDX_UNDEFINED)
      return fail("'field' references unknown field '%s'",nam);
    // create new linemap
    TLineMapDefinition *linemapP = new TLineMapDefinition(this,fid);
    // - save in list
    fLineMaps.push_back(linemapP);
    // - let element handle parsing
    expectChildParsing(*linemapP);
  }
  #ifdef EMAIL_FORMAT_SUPPORT
  else if (strucmp(aElementName,"mimemail")==0)
    expectBool(fMIMEMail);
  else if (strucmp(aElementName,"sizelimitfield")==0)
    expectFieldID(fSizeLimitField,fFieldListP);
  else if (strucmp(aElementName,"bodymimetypesfield")==0)
    expectFieldID(fBodyMIMETypesFid,fFieldListP);
  else if (strucmp(aElementName,"bodycountfield")==0)
    expectFieldID(fBodyCountFid,fFieldListP);
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  else if (strucmp(aElementName,"maxattachments")==0)
    expectInt16(fMaxAttachments);
  else if (strucmp(aElementName,"attachmentcountfield")==0)
    expectFieldID(fAttachmentCountFid,fFieldListP);
  else if (strucmp(aElementName,"attachmentmimetypesfield")==0)
    expectFieldID(fAttachmentMIMETypesFid,fFieldListP);
  else if (strucmp(aElementName,"attachmentsfield")==0)
    expectFieldID(fAttachmentContentsFid,fFieldListP);
  else if (strucmp(aElementName,"attachmentsizesfield")==0)
    expectFieldID(fAttachmentSizesFid,fFieldListP);
  else if (strucmp(aElementName,"attachmentnamesfield")==0)
    expectFieldID(fAttachmentNamesFid,fFieldListP);
  #endif
  #endif
  // - none known here
  else
    return TProfileConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TTextProfileConfig::localStartElement


// resolve
void TTextProfileConfig::localResolve(bool aLastPass)
{
  // nop
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TTextProfileConfig::localResolve

#endif


#ifdef HARDCODED_TYPE_SUPPORT

TLineMapDefinition *TTextProfileConfig::addLineMap(
  sInt16 aFid, sInt16 aNumLines, bool aAllowEmpty,
  bool aInHeader, const char* aHeaderTag,
  T822ValueType aValueType,
  char aListSeparator, sInt16 aMaxRepeat, sInt16 aRepeatInc
)
{
  // create new linemap
  TLineMapDefinition *linemapP = new TLineMapDefinition(this,aFid);
  // set properties
  linemapP->fNumLines=aNumLines;
  linemapP->fAllowEmpty=aAllowEmpty;
  linemapP->fInHeader=aInHeader;
  TCFG_ASSIGN(linemapP->fHeaderTag,aHeaderTag);
  // save email options
  linemapP->fValueType=aValueType;
  linemapP->fListSeparator=aListSeparator;
  linemapP->fListSeparator=aMaxRepeat;
  linemapP->fListSeparator=aRepeatInc;
  // save in list
  fLineMaps.push_back(linemapP);
  // return pointer
  return linemapP;
} // TTextProfileConfig::addLineMap

#endif


// handler factory
TProfileHandler *TTextProfileConfig::newProfileHandler(TMultiFieldItemType *aItemTypeP)
{
  // check if fieldlists match as they should
  if (aItemTypeP->getFieldDefinitions()!=fFieldListP) {
    // profile is for another field list, cannot be used for this item type
    return NULL;
  }
  // our handler is the text profile handler
  return (TProfileHandler *)(new TTextProfileHandler(this,aItemTypeP));
}



/*
 * Implementation of TTextProfileHandler
 */


TTextProfileHandler::TTextProfileHandler(
  TTextProfileConfig *aTextProfileCfgP,
  TMultiFieldItemType *aItemTypeP
) : TProfileHandler(aTextProfileCfgP, aItemTypeP)
{
  // save profile config pointer
  fProfileCfgP = aTextProfileCfgP;
  // datastore settable options defaults
  fNoAttachments = false;
  fItemSizeLimit = -1;
} // TTextProfileHandler::TTextProfileHandler


TTextProfileHandler::~TTextProfileHandler()
{
  // nop for now
} // TTextProfileHandler::~TTextProfileHandler


#ifdef OBJECT_FILTERING

// Filtering: add keywords and property names to filterCap
void TTextProfileHandler::addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDescriptor, TSyncItemType *aItemTypeP)
{
  // search linemaps for fields with keyword
  TLineMapList::iterator pos;
  for(pos=fProfileCfgP->fLineMaps.begin();pos!=fProfileCfgP->fLineMaps.end();pos++) {
    // first priority: compare with explicit filterkeyword, if any
    if (!TCFG_ISEMPTY((*pos)->fFilterKeyword)) {
      // has a filterkeyword, show it
      addPCDataStringToList(TCFG_CSTR((*pos)->fFilterKeyword), &aFilterKeywords);
    }
  }
} // TTextProfileHandler::addFilterCapPropsAndKeywords



// get field index of given filter expression identifier.
sInt16 TTextProfileHandler::getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex)
{
  // search linemaps for tagged fields
  TLineMapList::iterator pos;
  for(pos=fProfileCfgP->fLineMaps.begin();pos!=fProfileCfgP->fLineMaps.end();pos++) {
    // first priority: compare with explicit filterkeyword, if any
    if (!TCFG_ISEMPTY((*pos)->fFilterKeyword)) {
      // compare with filterkeyword
      if (strucmp(TCFG_CSTR((*pos)->fFilterKeyword),aIdentifier)==0)
        return (*pos)->fFid;
    }
    else if (TCFG_SIZE((*pos)->fHeaderTag)>1) { // one tag char and a separator at least
      // if no filterkeyword defined, compare to header without separator
      if (strucmp(TCFG_CSTR((*pos)->fHeaderTag),aIdentifier,TCFG_SIZE((*pos)->fHeaderTag)-1)==0)
        return (*pos)->fFid;
    }
  }
  // no field ID found in profile
  return FID_NOT_SUPPORTED;
} // TTextProfileHandler::getFilterIdentifierFieldIndex

#endif


#ifdef EMAIL_FORMAT_SUPPORT


// find token in header data
static cAppCharP findToken(cAppCharP aText, cAppCharP aTextEnd, char aStopChar, cAppCharP &aTokenStart, stringSize &aTokenLen)
{
  // skip trailing whitespace
  cAppCharP e=aTextEnd;
  if (!e)
    e=aText+strlen(aText);
  while (aText<e && isspace(*aText)) aText++;
  bool quoted = (*aText=='"'); // " to fool buggy colorizer
  if (quoted) {
    aText++;
  }
  // token starts here
  aTokenStart=aText;
  // find end
  while (aText<e) {
    if (quoted) {
      if (*aText=='"') break; // " to fool buggy colorizer
      if (*aText=='\\') {
        aText++;
        if (*aText)
          aText++; // do not interpret next
      }
    }
    else {
      if (*aText==aStopChar || isspace(*aText)) break;
    }
    aText++;
  }
  aTokenLen=aText-aTokenStart;
  // skip ending quote
  if (aText<e && quoted && *aText=='"') aText++; // " to fool buggy colorizer
  // advance to next non-space
  while (aText<e && isspace(*aText)) aText++;
  // return position
  return aText;
} // findToken


// check if line contains a MIME header
static bool checkMimeHeaders(cAppCharP aLine, stringSize aSize, string &aContentType, TEncodingTypes &aEncoding, uInt32 &aContentLen, TCharSets &aCharSet, string &aBoundary, string *aFileNameP)
{
  stringSize toksz,valtoksz;
  cAppCharP tok,valtok,p,e;
  sInt16 en;
  e=aLine+aSize;
  // search header name
  p=aLine;
  p=findToken(p,e,':',tok,toksz);
  if (*p!=':') return false; // no header
  ++p;
  // compare header name
  if (strucmp(tok,"Content-Type",toksz)==0) {
    // get content type
    p=findToken(p,e,';',tok,toksz);
    aContentType.assign(tok,toksz);
    // get parameters
    while (*p==';') {
      p++;
      p=findToken(p,e,'=',tok,toksz);
      if (*p=='=') {
        p++;
        // get value
        p=findToken(p,e,';',valtok,valtoksz);
        if (strucmp(tok,"charset",toksz)==0) {
          // charset
          if (StrToEnum(MIMECharSetNames, numCharSets, en, valtok, valtoksz))
            aCharSet=(TCharSets)en;
          else
            aCharSet=chs_unknown;
        }
        else if (strucmp(tok,"boundary",toksz)==0) {
          // boundary
          aBoundary.assign(valtok,valtoksz);
        }
      }
    } // while params
  }
  else if (strucmp(tok,"Content-Transfer-Encoding",toksz)==0) {
    // get encoding
    p=findToken(p,e,';',tok,toksz);
    if (StrToEnum(MIMEEncodingNames, numMIMEencodings, en, tok, toksz))
      aEncoding = (TEncodingTypes)en;
    else
      aEncoding = enc_none;
  }
  else if (strucmp(tok,"Content-Length",toksz)==0) {
    // get content length (not MIME, this is Synthesis' own invention, only valid for encoding=binary!!)
    p=findToken(p,e,';',tok,toksz);
    StrToULong(tok, aContentLen, toksz);
  }
  else if (aFileNameP && strucmp(tok,"Content-Disposition",toksz)==0) {
    // get disposition
    p=findToken(p,e,';',tok,toksz);
    aFileNameP->erase();
    if (strucmp(tok,"attachment",toksz)==0 || strucmp(tok,"inline",toksz)==0) {
      while (*p==';') {
        p++;
        p=findToken(p,e,'=',tok,toksz);
        if (strucmp(tok,"filename",toksz)==0) {
          // filename, get it
          if (*p=='=') {
            p++;
            p=findToken(p,e,';',tok,toksz);
            aFileNameP->assign(tok,toksz);
          }
        }
        else {
          // other param, skip it
          if (*p=='=') {
            p++;
            p=findToken(p,e,';',tok,toksz);
          }
        }
      }
    }
  }
  else return false; // no MIME header
  // found MIME header
  return true;
} // checkMimeHeaders


// parse body into appropriate field(s)
cAppCharP TTextProfileHandler::parseBody(
  cAppCharP aText, // body text to parse
  stringSize aTextSize, // max text to parse
  cAppCharP aType, TEncodingTypes aEncoding, uInt32 aContentLen, TCharSets aCharSet,
  TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP,
  cAppCharP aBoundary // boundary, NULL if none
)
{
  // empty boundary is no boundary
  if (aBoundary && *aBoundary==0) aBoundary=NULL;
  POBJDEBUGPRINTFX(fItemTypeP->getSession(),DBG_PARSE,(
    "Parsing body (part) with Content-Type='%s', Encoding=%s, contentLen=%ld, Charset=%s, Boundary='%s'",
    aType,
    MIMEEncodingNames[aEncoding],
    (long)aContentLen,
    MIMECharSetNames[aCharSet],
    aBoundary ? aBoundary : "<none>"
  ));
  // params for parts
  string contentType=aType; // content type
  string boundary; // main boundary
  string filename; // attachment
  TEncodingTypes encoding=aEncoding; // main encoding
  uInt32 contentlen=aContentLen; // content len in case encoding is binary
  TCharSets charset=aCharSet; // main charset
  // helpers
  string decoded;
  string converted;
  TItemField *fldP;
  // check if alternative
  bool alternative = strucmp(aType,"multipart/alternative")==0;
  bool foundBody=false;
  // search for first boundary occurrence if this is multipart
  cAppCharP toBeParsed=NULL; // pointer to previous part to be processed
  size_t previousSize=0;
  cAppCharP prevStart=NULL;
  cAppCharP p=aText;
  cAppCharP eot=p+aTextSize;
  bool startpart=false;
  // process parts
  do {
    startpart=false;
    // search for a boundary occurrence if this is multipart
    if (aBoundary) {
      size_t bl=strlen(aBoundary);
      prevStart=p;
      // check for special case: if we have something to parse, and encoding is binary,
      // use the content-length header to skip to next boundary
      if (toBeParsed && encoding==enc_binary) {
        // we simply KNOW how much to skip until next boundary starts
        p=toBeParsed+contentlen;
        // p now points to the next boundary indicator
      }
      // now detect boundary indicator
      do {
        if (p+2+bl<=eot && strucmp(p,"--",2)==0) {
          p+=2;
          if (strucmp(p,aBoundary,bl)==0) {
            // start of line matches boundary
            startpart=true;
            // calc how much we skipped until finding this boundary
            previousSize=p-2-prevStart;
            // A boundary has to be preceeded by a CRLF according to rfc2046, and
            // this CRLF belongs to the boundary and is NOT considered to be content of the preceeding part
            // Therefore, the following code is only needed for error tolerance reasons - for
            // correctly formatted message it's just a -=2.
            if (previousSize>2) {
              if (*(p-3)==0x0A || *(p-3)==0x0D) previousSize--;
              if (*(p-4)==0x0A || *(p-4)==0x0D) previousSize--;
            }
            // Note: in case of binary encoding, previousSize should be contentlen here!
            p+=bl;
          }
        }
        // skip until end of line
        while (p<eot && *p && *p!='\x0A') p++;
        if (p<eot && *p) p++; // skip LF as well
      } while (p<eot && *p && !startpart);
      // if we exit here and have no startpart, body does not contain expected parts
      // so exit here
      if (!startpart)
        return p;
      else {
        // altough previousSize should be equal to contentlen here, trust contentlen
        if (toBeParsed && encoding==enc_binary)
          previousSize=contentlen;
      }
    }
    // p is now start of space after found boundary if startpart is set
    // previousSize is now size of space between where we've started and
    // start of next boundary. If toBeParsed, this is a part, otherwise it's a preamble
    if (toBeParsed) {
      if (previousSize>0) {
        // parse leaf part (DO NOT CHANGE p HERE, it points to next part)
        // decide if this is body or attachment
        bool isText = contentType.empty() || (strucmp(contentType.c_str(),"text/",5)==0);
        if (isText && filename.empty()) {
          // this is a part of the body
          // - check if we can store multiple bodies
          bool multibody = aItem.getField(aLineMapP->fFid)->isArray();
          if (!foundBody || multibody) {
            sInt16 bodyIndex=VARIDX_UNDEFINED;
            // not found plain text body yet (or we can store multiple bodies)
            if (!alternative || strucmp(contentType.c_str(),"text/plain")==0) {
              // strictly plain text or not alternative, store it in the first body array element
              bodyIndex=0;
              // found plain text body, discard other alternatives if we have no other body variants
              if (alternative) foundBody=true;
            }
            else if (multibody) {
              // text, but not plain - and we can store alternatives, do it!
              bodyIndex=++fBodyAlternatives;
            }
            // if we shall store, do it now (otherwise, fldP is NULL)
            if (bodyIndex!=VARIDX_UNDEFINED) {
              // - get MIME-type field
              fldP = aItem.getArrayField(fProfileCfgP->fBodyMIMETypesFid,bodyIndex);
              if (fldP) {
                fldP->setAsString(contentType); // store MIME type
              }
              // - get content field
              fldP = aItem.getArrayField(aLineMapP->fFid,bodyIndex);
              if (fldP) {
                // - decode
                decoded.erase();
                decoded.reserve(previousSize); // we need approx this sizee -> reserve to speed up
                appendDecoded(
                  toBeParsed,
                  previousSize,
                  decoded,
                  encoding
                );
                // - convert charset
                converted.erase();
                converted.reserve(decoded.size()); // we need approx this size -> reserve to speed up
                appendStringAsUTF8(
                  decoded.c_str(),
                  converted,
                  charset,
                  lem_cstr
                );
                decoded.erase(); // we do not need this any more
                // add to field
                fldP->appendString(converted.c_str(), converted.size());
              }
            }
          }
        }
        else {
          // this is an attachment, ignore it if we have no attachment support
          #ifdef EMAIL_ATTACHMENT_SUPPORT
          if (filename.empty()) {
            // create filename
            StringObjPrintf(filename,"attachment%hd.bin",fExtraParts);
          }
          // save filename
          if (fProfileCfgP->fAttachmentNamesFid!=VARIDX_UNDEFINED) {
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentNamesFid,fExtraParts);
            if (fldP) {
              fldP->setAsString(filename.c_str());
            }
          }
          // save type
          if (fProfileCfgP->fAttachmentMIMETypesFid!=VARIDX_UNDEFINED) {
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentMIMETypesFid,fExtraParts);
            if (fldP) {
              fldP->setAsString(contentType.c_str());
            }
          }
          // now decode attachment itself
          decoded.erase();
          decoded.reserve(previousSize); // we need approx this sizee -> reserve to speed up
          appendDecoded(
            toBeParsed,
            previousSize,
            decoded,
            encoding
          );
          // save attachment data
          if (fProfileCfgP->fAttachmentContentsFid!=VARIDX_UNDEFINED) {
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentContentsFid,fExtraParts);
            if (fldP) {
              fldP->setAsString(decoded.c_str(),decoded.size());
            }
          }
          // save size in extra field
          if (fProfileCfgP->fAttachmentSizesFid!=VARIDX_UNDEFINED) {
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentSizesFid,fExtraParts);
            if (fldP) {
              fldP->setAsInteger(decoded.size());
            }
          }
          decoded.erase(); // we do not need this any more
          // done, found one extra part
          fExtraParts++;
          #else
          // Attachment cannot be processed
          // - get body field
          fldP = aItem.getArrayField(aLineMapP->fFid,0);
          // - append attachment replacement message
          string msg;
          if (filename.empty()) filename="<unnamed>";
          StringObjPrintf(msg,"\n\nAttachment not stored: %s\n\n",filename.c_str());
          fldP->appendString(msg.c_str());
          #endif
        }
      } // if not empty part
      // done
      toBeParsed=NULL;
      // if parsing single part, we're done
      if (!aBoundary) return p;
    }
    // check for more parts or exit if this was the closing boundary
    if (startpart) {
      // we have found a boundary above, p=space following it
      // part starts here
      bool isPart=false;
      // - get headers
      while (p<eot && *p && *p!='\x0D' && *p!='\x0A' && *p!='-') {
        // not end or empty line or start of other boundary, find end of next header line
        char c;
        cAppCharP q=p;
        while (q<eot && (c=*q++)) {
          if (c=='\x0D' || c=='\x0A') {
            if (c=='\x0D' && *q=='\x0A') {
              // CRLF sequence, do not count CR
            }
            else {
              // end of line, see if folded
              if (*q=='\x0D' || *q=='\x0A' || !isspace(*q)) {
                // not folded, end of line
                break;
              }
            }
          }
        }
        // check headers
        if (checkMimeHeaders(p,q-p,contentType,encoding,contentlen,charset,boundary,&filename))
          isPart=true; // at least one relevant header found, this is a part
        // next line
        p=q;
      }
      // now we have all the headers
      // - skip end of headers (CR)LF if any
      if (p<eot && *p=='\x0D') p++;
      if (p<eot && *p=='\x0A') p++;
      // - check for end of this multipart
      if (!isPart) {
        // this is not a part, end current multipart scanning
        // - return start of next boundary or end of text
        return p;
      }
      // we have now the valid headers for the next part
      // - if this is a multipart, recurse
      if (strucmp(contentType.c_str(),"multipart/",10)==0) {
        // recurse
        if (boundary.empty()) return NULL; // multipart w/o boundary is bad
        p = parseBody(
          p, // body text to parse
          eot-p, // max size to parse
          contentType.c_str(),encoding,contentlen,charset,
          aItem, aLineMapP,
          boundary.c_str() // boundary for nested parts
        );
      }
      else {
        // single leaf part, process it when we've found the next boundary
        toBeParsed=p;
      }
    } // if startpart
    else {
      // no start of a part by boundary
      if (!aBoundary) {
        // not part of a multipart, parse as is
        toBeParsed=p;
        previousSize=eot-p;
      }
    }
  } while(true);
} // parseBody

#endif

// parse header fields


// parse value into appropriate field(s)
bool TTextProfileHandler::parseContent(const char *aValue, stringSize aValSize, TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP)
{
  // get field
  TItemField *fieldP = aItem.getField(aLineMapP->fFid);
  // use appropriate translator
  string s;
  switch (aLineMapP->fValueType) {
    case vt822_timestamp:
      #ifdef EMAIL_FORMAT_SUPPORT
      // rfc822 timestamp
      if (!fieldP->isBasedOn(fty_timestamp)) break;
      s.assign(aValue,aValSize);
      if (!(static_cast<TTimestampField *>(fieldP)->setAsRFC822date(s.c_str(),aItem.getSession()->fUserTimeContext,false)))
        fieldP->assignEmpty();
      break;
      #endif
    case vt822_body:
      #ifdef EMAIL_FORMAT_SUPPORT
      // falls through to plain if email support is switched off
      if (fProfileCfgP->fMIMEMail) {
        // clear body text field
        fieldP->assignEmpty();
        fExtraParts=0;
        fBodyAlternatives=0;
        // now parse body (we should already have parsed the content headers
        parseBody(
          aValue, // body text to parse
          aValSize,
          fContentType.c_str(),fEncoding,fContentLen,fCharSet,
          aItem, aLineMapP,
          fBoundary.c_str() // boundary, NULL or empty if none
        );
        // save number of bodies
        if (fProfileCfgP->fBodyCountFid!=VARIDX_UNDEFINED) {
          TItemField *fldP=aItem.getField(fProfileCfgP->fBodyCountFid);
          if (fldP) {
            fldP->setAsInteger(fBodyAlternatives+1);
          }
        }
        // save number of attachments
        #ifdef EMAIL_ATTACHMENT_SUPPORT
        if (fProfileCfgP->fAttachmentCountFid!=VARIDX_UNDEFINED) {
          TItemField *fldP=aItem.getField(fProfileCfgP->fAttachmentCountFid);
          if (fldP) {
            fldP->setAsInteger(fExtraParts);
          }
        }
        #endif
        break;
      }
      #endif
      goto standardfield;
    case vt822_rfc2047:
      #ifdef EMAIL_FORMAT_SUPPORT
      // text field encoded according to RFC2047
      s.erase();
      appendRFC2047AsUTF8(aValue,aValSize,s);
      fieldP->setAsString(s.c_str());
      break;
      #else
      goto standardfield;
      #endif

    standardfield:
    case vt822_plain:
      // plain text
    default:
      // assign as string
      fieldP->setAsString(aValue,aValSize);
      break;
  }
  return true;
} // TTextProfileHandler::parseContent



#ifdef EMAIL_FORMAT_SUPPORT


// add a MIME-Boundary
static void addBoundary(string &aString, sInt16 aLevel, bool aForHeader=false)
{
  if (!aForHeader) aString+="\x0D\x0A--";
  StringObjAppendPrintf(aString,"--==========_=_nextpart_%03hd_42503735617.XE======",aLevel);
  if (!aForHeader) aString+="\x0D\x0A";
} // TTextProfileHandler::addBoundary


// add a body content-type header
static void addBodyTypeHeader(sInt16 aBodyTypeFid, sInt16 aBodyIndex, TMultiFieldItem &aItem, string &aString)
{
  string bodytype;
  TItemField *fldP = aItem.getArrayField(aBodyTypeFid,aBodyIndex,true);
  if (fldP && !fldP->isEmpty())
    fldP->getAsString(bodytype);
  else
    bodytype="text/plain";
  aString+="Content-Type: ";
  aString+=bodytype;
  aString+="; charset=\"UTF-8\"\x0D\x0A";
} // addBodyTypeHeader


// generate body (multipart/alternative if there is more than one)
// if aLevel==0, content type was already set before
bool TTextProfileHandler::generateBody(sInt16 aLevel, TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP, string &aString)
{
  // check if we need to add headers
  if (aLevel>0) {
    #ifdef EMAIL_ATTACHMENT_SUPPORT
    if (fBodyAlternatives>0) {
      // multiple bodies, nested multipart
      aString+="Content-Type: multipart/alternative;\x0D\x0A boundary=\"";
      addBoundary(aString,aLevel,true); // nested boundary
      aString+="\"\x0D\x0A";
    }
    else
    #endif
    {
      // single body, add body type
      addBodyTypeHeader(fProfileCfgP->fBodyMIMETypesFid, 0, aItem, aString);
      // - end part header
      aString+="\x0D\x0A";
    }
  }
  // now add body/bodies
  // - get size limit for this item
  sInt16 bodyindex=0;
  do {
    #ifdef EMAIL_ATTACHMENT_SUPPORT
    if (fBodyAlternatives>0 && fItemSizeLimit!=0) {
      // - opening boundary
      addBoundary(aString,aLevel); // nested
      // - content type
      addBodyTypeHeader(fProfileCfgP->fBodyMIMETypesFid, bodyindex, aItem, aString);
      // - end part header
      aString+="\x0D\x0A";
    }
    #endif
    // get body field
    TItemField *fieldP = aItem.getArrayField(aLineMapP->fFid,bodyindex);
    if (!fieldP)
      break; // should not happen
    // now generate body contents
    if (fItemSizeLimit==0) {
      // limited to nothing, just don't send anything
      fLimited=true;
      break;
    }
    else if (fieldP->isBasedOn(fty_string)) {
      TStringField *sfP = static_cast<TStringField *>(fieldP);
      if (fItemSizeLimit>0) {
        // limited string field
        // - check if we can add more
        if (fItemSizeLimit<=fGeneratedBytes) {
          // already exhausted, suppress body completely
          fLimited=true;
          break;
        }
        // add limited number of body bytes
        // - determine number of bytes to send
        #ifdef STREAMFIELD_SUPPORT
        sInt32 bodysize=sfP->getStreamSize();
        if (bodysize+fGeneratedBytes > fItemSizeLimit) {
          bodysize=fItemSizeLimit-fGeneratedBytes;
          fLimited=true;
        }
        // - get appropriate number of bytes
        char *bodyP = new char[bodysize+1];
        sfP->resetStream();
        bodysize = sfP->readStream(bodyP,bodysize);
        bodyP[bodysize]=0;
        // - append to content string
        aString.reserve(aString.size()+bodysize); // reserve what we need approximately
        appendUTF8ToString(
          bodyP,
          aString,
          chs_utf8, // always UTF8 for body
          lem_dos // CRLFs for email
        );
        // approximately, UTF-8 conversion and CRLF might cause slightly more chars
        fGeneratedBytes+=bodysize;
        // - get rid of buffer
        delete bodyP;
        #else
        // simply get it
        fieldP->appendToString(aString,fItemSizeLimit);
        fGeneratedBytes+=fieldP->getStringSize();
        #endif
      }
      else {
        // no limit, simply append to content string
        appendUTF8ToString(
          sfP->getCStr(),
          aString,
          chs_utf8, // always UTF8 for body
          lem_dos // CRLFs for email
        );
        // approximately, UTF-8 conversion and CRLF might cause slightly more chars
        fGeneratedBytes+=sfP->getStringSize();
      }
    }
    else {
      // no string field, just append string representation
      fieldP->appendToString(aString);
      fGeneratedBytes+=fieldP->getStringSize();
    }
    // done one body
    bodyindex++;
    // repeat until all done
  } while (bodyindex<=fBodyAlternatives && (fItemSizeLimit<0 || fGeneratedBytes<fItemSizeLimit));
  // now add final boundary if we had alternatives
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  if (fBodyAlternatives>0 && fItemSizeLimit!=0) {
    // - closing boundary for last part
    addBoundary(aString,aLevel); // nested
  }
  #endif
  return true;
} // TTextProfileHandler::generateBody

#endif


// generate contents of a header or body
// returns true if tagging and folding is needed on output,
// false if output can simply be appended to text (such as: no output at all)
bool TTextProfileHandler::generateContent(TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP, string &aString)
{
  aString.erase(); // nothing by default
  bool needsfolding=true;
  string s;

  // %%% missing repeats
  TItemField *fieldP=aItem.getField(aLineMapP->fFid);
  if (!fieldP) return false; // no field contents, do not even show the tag
  switch (aLineMapP->fValueType) {
    case vt822_body:
      // body with size restriction
      #ifdef EMAIL_FORMAT_SUPPORT
      #ifdef EMAIL_ATTACHMENT_SUPPORT
      // - multipart is supported
      if (fExtraParts>0) {
        // add body as first part
        // - opening boundary
        addBoundary(aString,0);
        // - add body on level 1 (means that it must add its own headers)
        generateBody(1,aItem,aLineMapP,aString);
        // - add attachments
        sInt16 attIdx;
        TItemField *fldP;
        for (attIdx=0; attIdx<fExtraParts; attIdx++) {
          // - get size of next attachment
          sInt32 sizebefore=aString.size(); // remember size before this attachment
          sInt32 attachsize=0;
          if (fProfileCfgP->fAttachmentSizesFid!=VARIDX_UNDEFINED) {
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentSizesFid,attIdx,true);
            if (!fldP) continue;
            // get it from separate field
            attachsize=fldP->getAsInteger();
          }
          else {
            // get it from attachment itself (will probably pull proxy)
            fldP=aItem.getArrayField(fProfileCfgP->fAttachmentContentsFid,attIdx,true);
            if (!fldP) continue;
            attachsize=fldP->getStringSize();
          }
          // - check if we have data for the attachment
          if (attachsize==0) continue;
          // Prepare attachment
          TItemField *attfldP = NULL; // none yet
          string attachMsg;
          attachMsg.erase(); // no message
          // - get content type
          bool isText=false;
          fldP = aItem.getArrayField(fProfileCfgP->fAttachmentMIMETypesFid,attIdx,true);
          string contenttype;
          if (fldP && !fldP->isEmpty()) {
            fldP->getAsString(contenttype);
          }
          else {
            contenttype="application/octet-stream";
          }
          // - check for text/xxxx contents
          if (strucmp(contenttype.c_str(),"text/",5)==0) isText=true;
          // - check if attachment has enough room
          if (
            (fItemSizeLimit>=0 && fGeneratedBytes+attachsize>fItemSizeLimit) || // limit specified by client
            !fItemTypeP->getSession()->dataSizeTransferable(fGeneratedBytes+attachsize*(isText ? 3 : 4)/3) // physical limit as set by maxMsgSize in SyncML 1.0 and maxObjSize in SyncML 1.1
          ) {
            // no room for attachment, include a text message instead
            fldP = aItem.getArrayField(fProfileCfgP->fAttachmentNamesFid,attIdx,true);
            string attnam;
            if (fldP)
              fldP->getAsString(attnam);
            else
              attnam="unnamed";
            StringObjPrintf(attachMsg,
              "\x0D\x0A" "Attachment suppressed: '%s' (%ld KBytes)\x0D\x0A",
              attnam.empty() ? "<unnamed>" : attnam.c_str(),
              long(attachsize/1024)
            );
            // set type
            contenttype="text/plain";
            isText=true; // force in-line
            // signal incomplete message
            // NOTE: other attachments that are smaller may still be included
            fLimited=true;
          }
          else {
            // we can send the attachment
            attfldP = aItem.getArrayField(fProfileCfgP->fAttachmentContentsFid,attIdx,true);
            if (!attfldP) continue; // cannot generate this attachment
          }
          // - opening boundary for attachment
          addBoundary(aString,0);
          // - add disposition
          aString+="Content-Disposition: ";
          if (isText) {
            // text is always in-line
            aString+="inline";
          }
          else {
            // non-text is attachment if it has a filename
            fldP = aItem.getArrayField(fProfileCfgP->fAttachmentNamesFid,attIdx,true);
            if (fldP && !fldP->isEmpty()) {
              // has a filename, make attachment
              aString+="attachment; filename=\"";
              fldP->appendToString(aString);
              aString+="\"";
            }
            else {
              // has no filename, show inline
              aString+="inline";
            }
          }
          aString+="\x0D\x0A";
          // - start content type (but no charset yet)
          aString+="Content-Type: ";
          aString+=contenttype;
          // - check attachment mode
          if (attfldP && attfldP->isBasedOn(fty_blob)) {
            // Attachment is a BLOB, so it may contain binary data
            TBlobField *blobP = static_cast<TBlobField *>(attfldP);
            // make sure charset/encoding are valid
            blobP->makeContentsValid();
            // - get charset from the BLOB
            if (blobP->fCharset!=chs_unknown) {
              aString+="; charset=\"";
              aString+=MIMECharSetNames[blobP->fCharset];
              aString+='"';
            }
            aString+="\x0D\x0A";
            // - if known, use originally requested encoding
            TEncodingTypes enc = blobP->fWantsEncoding;
            TEncodingTypes hasenc = blobP->fHasEncoding;
            // - make sure we use a valid encoding that is ok for sending as text
            if (enc==enc_b || enc==enc_none || enc==enc_binary) {
              enc = isText ? enc_8bit : enc_base64;
            }
            // - see if we should transmit the existing encoding
            if (isText) {
              if (hasenc==enc_7bit && hasenc==enc_8bit && hasenc==enc_quoted_printable)
                enc=hasenc; // already encoded
            }
            else {
              if (hasenc==enc_base64 || hasenc==enc_b)
                enc=hasenc; // already encoded
            }
            // when we are in Synthesis-special mode, and encoding is WBXML, we can use plain binary encoding
            // (specifying the length with a Content-Length: header)
            if (fItemTypeP->getTypeConfig()->fBinaryParts && fItemTypeP->getSession()->getEncoding()==SML_WBXML && (enc==enc_b || enc==enc_base64)) {
              // switch to 1:1 binary
              enc=enc_binary;
              StringObjAppendPrintf(aString,"Content-Length: %ld\x0D\x0A", long(blobP->getStringSize()));
            }
            // - set transfer encoding from the BLOB
            aString+="Content-Transfer-Encoding: ";
            aString+=MIMEEncodingNames[enc];
            aString+="\x0D\x0A";
            // - end of part headers
            aString+="\x0D\x0A";
            // - now add contents as-is (this pulls the proxy now)
            appendEncoded(
              (const uInt8 *)blobP->getCStr(), // input
              blobP->getStringSize(),
              aString, // append output here
              enc==hasenc ? enc_none : enc, // desired encoding if not already encoded
              MIME_MAXLINESIZE, // limit to standard MIME-linesize
              0, // current line size
              false // insert CRLFs for line breaks
            );
          }
          else {
            // Attachment isn't a BLOB, but a string. Transmit as 8bit, UTF-8
            aString+="; charset=\"";
            aString+=MIMECharSetNames[chs_utf8];
            aString+="\"\x0D\x0A";
            // - content encoding is 8bit
            aString+="Content-Transfer-Encoding: ";
            aString+=MIMEEncodingNames[enc_8bit];
            aString+="\x0D\x0A";
            // - end of part headers
            aString+="\x0D\x0A";
            // - simply append string
            if (attfldP) {
              attfldP->getAsString(s);
              appendUTF8ToString(
                s.c_str(),
                aString,
                chs_utf8, // always UTF8 for body
                lem_dos // CRLFs for email
              );
            }
            else {
              // append attachment suppression message
              aString+=attachMsg; // no attachment field, append replacement text instead
            }
          }
          // count added bytes
          fGeneratedBytes+=(aString.size()-sizebefore);
        } // for all attachments
        // - closing boundary
        addBoundary(aString,0);
      }
      else
      #endif
      {
        // message consists only of a body (which might have alternatives)
        // - add body on level 0 (means that it must not have own headers)
        generateBody(0,aItem,aLineMapP,aString);
      }
      // Body does not need any folding
      needsfolding=false;
      break;
      #else
      // no EMAIL FORMAT support
      goto standardfield;
      #endif
    case vt822_rfc2047:
      // text field encoded according to RFC2047
      #ifdef EMAIL_FORMAT_SUPPORT
      if (fieldP->isUnassigned()) return false; // field not assigned, do not even show the tag
      fieldP->getAsString(s);
      appendUTF8AsRFC2047(s.c_str(),aString);
      break;
      #else
      goto standardfield;
      #endif
    case vt822_timestamp:
      if (fieldP->isUnassigned()) return false; // field not assigned, do not even show the tag
      #ifdef EMAIL_FORMAT_SUPPORT
      if (!fieldP->isBasedOn(fty_timestamp)) break;
      static_cast<TTimestampField *>(fieldP)->getAsRFC822date(aString,aItem.getSession()->fUserTimeContext,true);
      break;
      #endif

#ifndef EMAIL_FORMAT_SUPPORT
    standardfield:
#endif
    case vt822_plain:
      // plain text
      if (fieldP->isUnassigned()) return false; // field not assigned, do not even show the tag
      fieldP->getAsString(aString);
      break;
  case num822ValueTypes:
      // not handled?
      break;
  }
  return needsfolding;
} // TTextProfileHandler::generateContent


// generate Data item (includes header and footer)
void TTextProfileHandler::generateText(TMultiFieldItem &aItem, string &aString)
{
  TLineMapList::iterator pos;

  // reset byte counter
  fGeneratedBytes=0;
  fLimited=false;
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  fExtraParts=0;
  #endif
  #ifdef EMAIL_FORMAT_SUPPORT
  fBodyAlternatives=0;
  bool multipart=false;
  #endif

  #ifdef SYDEBUG
  POBJDEBUGPRINTFX(fItemTypeP->getSession(),DBG_GEN+DBG_HOT,("Generating...."));
  aItem.debugShowItem(DBG_DATA+DBG_GEN);
  #endif

  // init attachment limit
  // - get from datastore if one is related
  if (fRelatedDatastoreP) {
    fItemSizeLimit = fRelatedDatastoreP->getItemSizeLimit();
    fNoAttachments = fRelatedDatastoreP->getNoAttachments();
  }
  // - if size limit is zero or attachments explicitly disabled,
  //   attachments are not allowed for this item
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  if (fItemSizeLimit==0 || fNoAttachments)
    fAttachmentLimit=0; // no attachments
  else
    fAttachmentLimit=fProfileCfgP->fMaxAttachments; // use limit from datatype config
  #endif
  // generate according to linemaps
  bool header = (*fProfileCfgP->fLineMaps.begin())->fInHeader; // we are in header if first is in header
  for (pos=fProfileCfgP->fLineMaps.begin();pos!=fProfileCfgP->fLineMaps.end();pos++) {
    // get linemap config
    TLineMapDefinition *linemapP = *pos;
    // separate body
    if (header && !linemapP->fInHeader) {
      // add special email headers
      #ifdef EMAIL_FORMAT_SUPPORT
      if (fProfileCfgP->fMIMEMail) {
        // basic support
        TItemField *cntFldP;
        #ifdef EMAIL_ATTACHMENT_SUPPORT
        // attachments allowed, get number
        cntFldP = aItem.getField(fProfileCfgP->fAttachmentCountFid);
        if (cntFldP && !cntFldP->isEmpty()) {
          // we have a count field, get it's value
          fExtraParts=cntFldP->getAsInteger();
        }
        else {
          // determine exta part number by counting attachments
          if (fProfileCfgP->fAttachmentContentsFid) {
            fExtraParts=aItem.getField(fProfileCfgP->fAttachmentContentsFid)->arraySize();
          }
        }
        // limit to what is allowed
        if (fExtraParts > fProfileCfgP->fMaxAttachments) fExtraParts=fProfileCfgP->fMaxAttachments;
        if (fExtraParts > fAttachmentLimit) { fExtraParts=fAttachmentLimit; fLimited=true; }
        // check if we have body alternatives
        cntFldP = aItem.getField(fProfileCfgP->fBodyCountFid);
        if (cntFldP && !cntFldP->isEmpty()) {
          // we have a count field, get it's value
          fBodyAlternatives=cntFldP->getAsInteger()-1;
        }
        else {
          fBodyAlternatives = aItem.getField(linemapP->fFid)->arraySize()-1;
        }
        if (fBodyAlternatives<0) fBodyAlternatives=0;
        // now add multipart content header if we have extra parts to send
        if (fExtraParts>0) {
          // we have attachments, this will be a multipart/mixed
          aString+="Content-Type: multipart/mixed;\x0D\x0A boundary=\"";
          addBoundary(aString,0,true);
          aString+="\"\x0D\x0A";
          multipart=true;
        }
        else if (fBodyAlternatives) {
          // no attachments, but multiple bodies
          aString+="Content-Type: multipart/alternative;\x0D\x0A boundary=\"";
          addBoundary(aString,0,true);
          aString+="\"\x0D\x0A";
          multipart=true;
        }
        else {
          // no attachments and no body alternatives, single body
          // - set type header for first and only part
          addBodyTypeHeader(fProfileCfgP->fBodyMIMETypesFid, 0,aItem,aString);
        }
        #else
        // only single body supported, always UTF-8
        addBodyTypeHeader(fProfileCfgP->fBodyMIMETypesFid, 0,aItem,aString);
        #endif
        // now add encoding header, always 8-bit
        aString+="Content-Transfer-Encoding: 8BIT\x0D\x0A";
      }
      else {
        // no mail format, end headers here
        aString.append("\x0D\x0A"); // extra empty line
      }
      #else
      // end headers
      aString.append("\x0D\x0A"); // extra empty line
      #endif
    }
    // generate value
    string fval;
    bool tagandfold=generateContent(aItem,linemapP,fval);
    // prevent empty ones if selected
    if (fval.empty() && !linemapP->fAllowEmpty) continue;
    // prefix with tag if any
    bool tagged=!TCFG_ISEMPTY(linemapP->fHeaderTag);
    // add field contents now
    // generate contents from field
    if (!tagandfold) {
      // no folding necessary
      #ifdef EMAIL_FORMAT_SUPPORT
      // - add updated limit header here if message is really limited
      if (fProfileCfgP->fMIMEMail) {
        if (fProfileCfgP->fSizeLimitField!=VARIDX_UNDEFINED) {
          TItemField *fldP = aItem.getField(fProfileCfgP->fSizeLimitField);
          fieldinteger_t limit = fItemSizeLimit;
          // now update its value
          if (!fLimited) limit=-1;
          fldP->setAsInteger(limit); // limited to specified size
          // now add the updated limit header (we have no linemap for it)
          aString+=X_LIMIT_HEADER_NAME ": ";
          fldP->appendToString(aString);
          aString+="\x0D\x0A"; // end of header
        }
        // terminate header not before here
        if (header && !linemapP->fInHeader) {
          // end headers
          aString.append("\x0D\x0A"); // extra empty line
        }
      }
      #endif
      // - fVal includes everything needed INCLUDING tag AND CRLF at end
      //   or fVal is empty meaning that the value does not need to be added at all
      aString.append(fval);
    }
    else {
      // add tag if tagged line
      if (tagged) {
        aString.append(linemapP->fHeaderTag);
        aString+=' '; // this extra space is common usage in RFC822 mails
      }
      // add with folding
      const char *p = fval.c_str();
      sInt16 n=(*pos)->fNumLines;
      sInt16 i=0;
      sInt16 cnt=0; // char counter
      sInt16 lastLWSP=-1; // no linear whitespace found yet
      char c;
      // add multi-line field contents
      while ((c=*p++)) {
        if (c=='\r') continue; // ignore CRs
        if (tagged) {
          // apply RFC822 folding (65 recommened for old terminals, 72 max)
          if (cnt>=65) {
            // check where we can fold
            if (lastLWSP>=0) {
              // this is the last LWSP
              // - new size of line is string beginning with lastLWSP up to end of string
              cnt=aString.size()-lastLWSP;
              // - insert a CRLF before the last LWSP
              aString.insert(lastLWSP,"\x0D\x0A");
              // - this one is now invalid
              lastLWSP=-1; // invalidate again
            }
          }
          if (isspace(c)) {
            // remember possible position for folding
            lastLWSP=aString.size(); // index of this LWSP
          }
        }
        if (c=='\n') {
          // line break in data
          if (tagged) {
            // for tagged fields, line break in data is used as recommended folding
            // position, so just fold NOW
            aString.append("\x0D\x0A ");
            lastLWSP=-1;
            cnt=1; // the space is already here
          }
          else {
            // for non-tagged fields, we might cut writing data here if no more lines allowed
            // - check if more lines allowed
            if (i<n || n==0) {
              // one more line allowed
              aString.append("\x0D\x0A");
              i++; // count the line
            }
          }
        }
        else {
          aString+=c; // append char as is
          cnt++;
        }
      }
      // one line at least
      aString.append("\x0D\x0A");
    }
  }
  #ifdef SYDEBUG
  POBJDEBUGPRINTFX(fItemTypeP->getSession(),DBG_GEN,("Generated:"));
  if (fItemTypeP->getDbgMask() & DBG_GEN) {
    // note, do not use debugprintf because string is too long
    POBJDEBUGPUTSXX(fItemTypeP->getSession(),DBG_GEN+DBG_USERDATA,aString.c_str(),0,true);
  }
  #endif
} // TTextProfileHandler::generateText


// parse Data item (includes header and footer)
bool TTextProfileHandler::parseText(const char *aText, stringSize aTextSize, TMultiFieldItem &aItem)
{
  TLineMapList::iterator pos;

  // get options from datastore if one is related
  if (fRelatedDatastoreP) {
    fItemSizeLimit = fRelatedDatastoreP->getItemSizeLimit();
    fNoAttachments = fRelatedDatastoreP->getNoAttachments();
  }
  // parse according to linemaps
  pos=fProfileCfgP->fLineMaps.begin();
  if (pos==fProfileCfgP->fLineMaps.end()) return true; // simply return, no mappings defined
  // - we are in header if first is in header
  bool header=(*pos)->fInHeader;
  #ifdef EMAIL_FORMAT_SUPPORT
  fContentType.erase(); // no known type
  fBoundary.erase(); // no boundary yet
  fEncoding=enc_8bit; // 8 bit
  fContentLen=0; // not defined until we have Synthesis-style binary encoding in parts
  fCharSet=chs_utf8; // UFT-8 is SyncML default
  fContentType.erase(); // no main content type
  #endif
  // - header has tagged fields if first has a tag
  bool tagged=!TCFG_ISEMPTY((*pos)->fHeaderTag);
  const char *p = aText;
  const char *eot = aText+aTextSize;
  // if we are starting in body, simulate a preceeding EOLN
  bool lastwaseoln=!header;
  while (pos!=fProfileCfgP->fLineMaps.end()) {
    // check special case of this linemap eating all of the remaining body text
    if (!header && (*pos)->fNumLines==0) {
      // Optimization: this linemap will receive the entire remainder of the message
      parseContent(p, eot-p, aItem, *pos);
      // and we are done
      goto parsed;
    }
    // scan input data
    string fval;
    fval.erase();
    char c=0;
    sInt16 i=0,n=0;
    bool assignnow=false;
    bool fielddone=false;
    while (p<=eot) {
      // get char, simulate a NUL if we are at end of text
      c = (p==eot) ? 0 : *p;
      p++; // make sure we're over eot now
      // convert all types of line ends: 0A, 0D0A and 0D are allowed
      // special 0D0D0A that sometimes happens (CRLF saved through a DOS
      // linefeed expander) is also detected correctly
      if (c==0x0D) {
        // CR, discard LF if one follows
        if (p<eot && *p==0x0A) {
          p++; // discard
          // check if previous char was 0D as well
          if (lastwaseoln && *(p-3)==0x0D) {
            // this is the famous 0D0D0A sequence
            continue; // simply completely ignore it, as previous CR was already detected as line end
          }
        }
        c='\n'; // internal line end char
      }
      else if (c==0x0A)
        c='\n'; // single LF is treated as line end char as well
      // process now
      if (c==0 || c=='\n') {
        // end of input line, if tagged headers, process line but ONLY if it's not an empty line
        if (header && tagged && !lastwaseoln) {
          // check if we have the entire line already
          if (p>=eot || c==0 || (*p!=' ' && *p!='\t')) {
            // end of text or next line does not begin with space or TAB -> end of header
            #ifdef EMAIL_FORMAT_SUPPORT
            if (fProfileCfgP->fMIMEMail) {
              // check for MIME-content relevant mail headers first
              if (!checkMimeHeaders(fval.c_str(),fval.size(),fContentType,fEncoding,fContentLen,fCharSet,fBoundary,NULL)) {
                // check for X-Sync-Limit special header
                const char *h = fval.c_str();
                const char *e = h+fval.size();
                const char *tok;
                stringSize toksz;
                h=findToken(h,e,':',tok,toksz);
                if (*h==':') {
                  // header field
                  ++h;
                  // check name
                  if (strucmp(tok,X_LIMIT_HEADER_NAME,toksz)==0) {
                    // X-Sync-Limit special header
                    h=findToken(h,e,':',tok,toksz);
                    TItemField *limfldP = aItem.getField(fProfileCfgP->fSizeLimitField);
                    if (limfldP)
                      limfldP->setAsString(tok,toksz);
                  }
                }
              }
            }
            // note that mime headers can still be mapped to fields, so fall through
            #endif
            // - search by tag for matching linemap now
            TLineMapList::iterator tagpos;
            for (tagpos=fProfileCfgP->fLineMaps.begin();tagpos!=fProfileCfgP->fLineMaps.end();tagpos++) {
              TCFG_STRING &s = (*tagpos)->fHeaderTag;
              if ((*tagpos)->fInHeader && !TCFG_ISEMPTY(s)) {
                if (strucmp(fval.c_str(),TCFG_CSTR(s),TCFG_SIZE(s))==0) {
                  // tag matches, set position to matching linemap
                  pos=tagpos;
                  // remove tag from input data
                  fval.erase(0,TCFG_SIZE(s));
                  // remove leading spaces
                  size_t j=0;
                  while (fval.size()>j && isspace(fval[j])) j++;
                  if (j>0) fval.erase(0,j);
                  // assign value now
                  assignnow=true;
                  break; // break for loop
                }
              }
            } // search for correct map
            // assignnow is set if we have found a map now, otherwise, header will be ignored
            fielddone=true; // cause loop exit, but first check for transition from header to body and set lastwaseoln
          }
          else {
            // process possible folding
            if (p<eot && c!=0 && isspace(*p)) {
              // this is a lineend because of folding -> ignore line end and just keep LWSP
              fval+=*p++; // keep the LWSP
              lastwaseoln=false; // last was LWSP, not EOLN :-)
              continue; // just check next one
            }
          }
        }
        // - check for switch from header to body
        if (header && lastwaseoln) {
          // two line ends in succession = end of header
          header=false;
          if (tagged) {
            // find first non-header linemap (pos can be anywhere within header linemaps here
            while (pos!=fProfileCfgP->fLineMaps.end() && (*pos)->fInHeader) pos++;
            // but no need to store, as tagged headers have stored already
          }
          else {
            // end of untagged headers
            // assign what is already accumulated
            assignnow=true;
          }
          tagged=false;
          // just stop here if no more linemaps
          if (pos==fProfileCfgP->fLineMaps.end()) {
            goto parsed; // do not spend time and memory with parsing unneeded data
          }
          // important optimization: if the last linemap does not have a line
          // count restriction, process rest of text without filling it into a string var
          if ((*pos)->fNumLines==0) {
            // this linemap will receive the entire remainder of the message
            parseContent(p, eot-p, aItem, *pos);
            // and we are done
            goto parsed;
          }
          break;
        }
        lastwaseoln=true;
        // end of input line
        if (!tagged) {
          i++; // count line
          n=(*pos)->fNumLines;
          if ((i>=n && n!=0) || c==0) {
            // line count exhausted or end of input text, assign to field now
            assignnow=true; // assign fval to field now
            break;
          }
          else
            if (c) fval+='\n'; // multi-line field, eoln if not eostring
        }
      } // if end of input line
      else {
        // not end of input line
        lastwaseoln=false;
        // add to value
        fval+=c;
      }
      if (!c || fielddone) break;
    }
    // assign accumulated value
    if (assignnow) {
      // assign according to linemap
      parseContent(fval.c_str(), fval.size(), aItem, *pos);
      // advance to next map if not in tagged header mode
      if (!tagged) pos++;
    }
    // all parsed, rest of definitions is not relevant, p is invalid
    if (c==0) break;
  } // while
parsed:
  #ifdef SYDEBUG
  POBJDEBUGPRINTFX(fItemTypeP->getSession(),DBG_PARSE,("Successfully parsed: "));
  if (fItemTypeP->getDbgMask() & DBG_PARSE) {
    // very detailed
    POBJDEBUGPUTSXX(fItemTypeP->getSession(),DBG_PARSE+DBG_USERDATA+DBG_EXOTIC,aText,0,true);
  }
  aItem.debugShowItem(DBG_DATA+DBG_PARSE);
  #endif
  return true;
} // TTextProfileHandler::parseData


/* end of TTextProfileHandler implementation */

// eof
