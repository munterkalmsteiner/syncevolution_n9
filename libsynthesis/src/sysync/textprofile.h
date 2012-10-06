/*
 *  File:         textprofile.h
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

#ifndef TextProfile_H
#define TextProfile_H

// includes
#include "syncitemtype.h"
#include "multifielditemtype.h"


namespace sysync {

typedef enum {
  vt822_plain,     // no conversion at all
  vt822_timestamp, // RFC2822 timestamp value
  vt822_body,      // email body
  vt822_rfc2047,   // text with RFC2047 MIME message header extensions
  num822ValueTypes
} T822ValueType;


// line map definition
class TLineMapDefinition : public TConfigElement {
  typedef TConfigElement inherited;
public:
  // constructor/destructor
  TLineMapDefinition(TConfigElement *aParentElementP, sInt16 aFid);
  virtual ~TLineMapDefinition();
  // properties
  // - number of lines (0=all)
  sInt16 fNumLines;
  // - allow empty
  bool fAllowEmpty;
  // - only header lines
  bool fInHeader;
  // - tagged header
  TCFG_STRING fHeaderTag; // must include the separator char (for RFC822 emails, a colon)
  // - field id to put text into
  sInt16 fFid;
  // RFC2822 parsing properties
  // - type of value
  T822ValueType fValueType;
  // - list separator (0=no list)
  char fListSeparator;
  // - max number of repeats (items in list), REP_ARRAY=unlimited, for array fields
  sInt16 fMaxRepeat;
  sInt16 fRepeatInc;
  #ifdef OBJECT_FILTERING
  // - no filterkeyword
  TCFG_STRING fFilterKeyword;
  #endif
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
}; // TLineMapDefinition


// line mapping definitions
typedef std::list<TLineMapDefinition *> TLineMapList;

// Text based datatype
class TTextProfileConfig : public TProfileConfig
{
  typedef TProfileConfig inherited;
public:
  TTextProfileConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TTextProfileConfig();
  // handler factory
  virtual TProfileHandler *newProfileHandler(TMultiFieldItemType *aItemTypeP);
  // properties
  // - Note, field list is parsed here, but is a property of TMultiFieldTypeConfig
  // - text-line based field definitions
  TLineMapList fLineMaps;
  #ifdef EMAIL_FORMAT_SUPPORT
  // email-specific properties
  // - if set, MIME mail extra feature according to RFC 2822/2045/2046 will be used
  bool fMIMEMail;
  // - field containing body type(s) (for body alternatives, if undefined defaults to text/plain)
  sInt16 fBodyMIMETypesFid;
  // - field that contains body count
  sInt16 fBodyCountFid;
  //   - overall body size limit field
  sInt16 fSizeLimitField;
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  // - max number of attachments (for non-arrays)
  sInt16 fMaxAttachments;
  // - field that contains attachment count
  sInt16 fAttachmentCountFid;
  // - field base offsets (or array field ID) for attachment components
  //   - attachment MIME types
  sInt16 fAttachmentMIMETypesFid;
  //   - attachment contents
  sInt16 fAttachmentContentsFid;
  //   - attachment sizes (in bytes)
  sInt16 fAttachmentSizesFid;
  //   - attachment (file)names
  sInt16 fAttachmentNamesFid;
  #endif
  #endif
  // public functions
  #ifdef HARDCODED_TYPE_SUPPORT
  TLineMapDefinition *addLineMap(
    sInt16 aFid, sInt16 aNumLines, bool aAllowEmpty,
    bool aInHeader=false, const char* aHeaderTag=NULL,
    T822ValueType aValueType=vt822_plain,
    char aListSeparator=0, sInt16 aMaxRepeat=1, sInt16 aRepeatInc=1
  );
  #endif
protected:
  #ifdef CONFIGURABLE_TYPE_SUPPORT
  // check config elements
public:
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
protected:
  virtual void localResolve(bool aLastPass);
  #endif
  virtual void clear();
}; // TTextProfileConfig


class TTextProfileHandler : public TProfileHandler
{
  typedef TProfileHandler inherited;
public:
  // constructor
  TTextProfileHandler(
    TTextProfileConfig *aTextProfileCfgP,
    TMultiFieldItemType *aItemTypeP
  );
  // destructor
  virtual ~TTextProfileHandler();
  #ifdef OBJECT_FILTERING
  // filtering
  // - get field index of given filter expression identifier.
  virtual sInt16 getFilterIdentifierFieldIndex(const char *aIdentifier, uInt16 aIndex);
  // - add keywords and property names to filterCap
  virtual void addFilterCapPropsAndKeywords(SmlPcdataListPtr_t &aFilterKeywords, SmlPcdataListPtr_t &aFilterProps, TTypeVariantDescriptor aVariantDesc, TSyncItemType *aItemTypeP);
  #endif
  // generate Text Data (includes header and footer)
  virtual void generateText(TMultiFieldItem &aItem, string &aString);
  // parse Data item (includes header and footer)
  virtual bool parseText(const char *aText, stringSize aTextSize, TMultiFieldItem &aItem);
private:
  // Internal routines
  #ifdef EMAIL_FORMAT_SUPPORT
  bool generateBody(sInt16 aLevel, TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP, string &aString);
  cAppCharP parseBody(
    cAppCharP aText, // body text to parse
    stringSize aTextSize, // max text to parse
    cAppCharP aType, TEncodingTypes aEncoding, uInt32 aContentLen, TCharSets aCharSet,
    TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP,
    cAppCharP aBoundary // boundary, NULL if none
  );
  #endif
  bool generateContent(TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP, string &aString);
  bool parseContent(const char *aValue, stringSize aValSize, TMultiFieldItem &aItem, TLineMapDefinition *aLineMapP);
  // vars
  TTextProfileConfig *fProfileCfgP; // the text profile config element
  fieldinteger_t fGeneratedBytes; // number of bytes already generated (for limit checks)
  bool fLimited; // set if size was actually limited
  // externally set options
  bool fNoAttachments;
  fieldinteger_t fItemSizeLimit;
  #ifdef EMAIL_FORMAT_SUPPORT
  sInt16 fExtraParts; // number of extra parts in addition to body
  sInt16 fBodyAlternatives; // number of body alternatives
  string fContentType; // main content type
  uInt32 fContentLen; // content len for Synthesis-style binary parts
  string fBoundary; // main boundary
  TEncodingTypes fEncoding; // main encoding
  TCharSets fCharSet; // main charset
  #ifdef EMAIL_ATTACHMENT_SUPPORT
  sInt16 fAttachmentLimit; // limit for attachments in this message
  #endif
  #endif
}; // TTextProfileHandler


} // namespace sysync

#endif  // TextProfile_H

// eof
