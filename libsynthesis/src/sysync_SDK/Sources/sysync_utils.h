/*
 *  File:         sysync_utils.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Provides some helper functions interfacing between SyncML Toolkit
 *  and C++ plus other utilities
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-16 : luz : created
 *
 */

#ifndef SYSYNC_UTILS_H
#define SYSYNC_UTILS_H

#include "sysync_globs.h"
// include external utils in separate files
#include "sysync_b64.h"
#include "sysync_md5.h"
#ifndef SYSYNC_ENGINE
#include "stringutil.h"
#endif
#include "lineartime.h"
#include "iso8601.h"

#ifndef FULLY_STANDALONE
#include "sysync.h"
#endif

#include "sml.h"
#include "smldevinfdtd.h"
#include "smlmetinfdtd.h"

namespace sysync {

#ifdef SYSYNC_TOOL

// convert between character sets
int charConv(int argc, const char *argv[]);

// parse RFC 2822 addr spec
int parse2822AddrSpec(int argc, const char *argv[]);

#endif


// max line size for MIME content (used while encoding and folding)
#define MIME_MAXLINESIZE 75


// supported charsets
typedef enum {
  chs_unknown, // invalid
  chs_ascii, // 7 bit ASCII-only, with nearest char conversion for umlauts etc.
  chs_ansi,
  chs_iso_8859_1,
  chs_utf8,
  chs_utf16,
  #ifdef CHINESE_SUPPORT
  chs_gb2312,
  chs_cp936,
  #endif
  numCharSets
} TCharSets;
// Note: Char set names are defined after this enum in other files,
// such as MimeDirItemType


// supported MIME encoding types
typedef enum {
  enc_none,
  enc_7bit,
  enc_8bit,
  enc_binary,
  enc_quoted_printable,
  enc_base64, // b64 including terminating with CRLF at end
  enc_b, // b64 without termination (as needed in RFC2047)
  numMIMEencodings
} TEncodingTypes;



// line end modes
typedef enum {
  lem_none, // none specified
  lem_unix, // 0x0A
  lem_mac,  // 0x0D
  lem_dos,  // 0x0D 0x0A
  lem_cstr, // as in C strings, '\n' which is 0x0A normally (but might be 0x0D on some platforms)
  lem_filemaker, // 0x0B (filemaker tab-separated text format, CR is shown as 0x0B within fields
  numLineEndModes
} TLineEndModes;

extern const char * const lineEndModeNames[numLineEndModes];


// literal quoting modes
typedef enum {
  qm_none, // none specified
  qm_duplsingle, // single quote must be duplicated
  qm_dupldouble, // double quote must be duplicated
  qm_backslash, // C-string-style escapes of CR,LF,TAB,BS,\," and ' (but no full c-string escape with \xXX etc.)
  numQuotingModes
} TQuotingModes;

extern const char * const quotingModeNames[numQuotingModes];



/*
The value of this element SHOULD BE one of bin, bool, b64, chr, int,
node, null or xml. If the element type is missing, the default value is chr. If the value is
bin, then the format of the content is binary data. If the value is bool, then the format of
the content is either true or false. If the value is b64, then the format of the content
information is binary data that has been character encoded using the Base64 transfer
encoding defined by [RFC2045]. If the value is chr, then the format of the content
information is clear-text in the character set specified on either the transport protocol, the
MIME content type header or the XML prolog. If the value is int, then the format of the
content information is numeric text representing an unsigned integer between zero and
2**32-1. If the value is node, then the content represents an interior object in the
management tree. If the value is null, then there is no content information. This value is
used by some synchronization data models to delete the content, but not the presence of
the property. If the value is xml, then the format of the content information is XML
structured mark-up data.
*/

// format types
typedef enum {
  fmt_chr, // default
  fmt_bin,
  fmt_b64,
  numFmtTypes
} TFmtTypes;

extern const char * const encodingFmtNames[numFmtTypes];
extern const char * const encodingFmtSyncMLNames[numFmtTypes];

extern const char * const MIMEEncodingNames[numMIMEencodings];
extern const char * const MIMECharSetNames[numCharSets];


// field (property) data types
typedef enum {
  proptype_chr, // Character
  proptype_int, // Integer
  proptype_bool, // Boolean
  proptype_bin, // Binary
  proptype_datetime, // Date and time of day
  proptype_phonenum, // Phone number
  proptype_text, // plain text
  proptype_unknown, // unknown
  numPropDataTypes
} TPropDataTypes;

extern const char * const propDataTypeNames[numPropDataTypes];


// Authorization types
typedef enum {
  auth_none,
  auth_basic,
  auth_md5,
  numAuthTypes
} TAuthTypes;

extern const char * const authTypeSyncMLNames[numAuthTypes];
//extern const char * const authFormatNames[numAuthTypes];


// char that is used for non-convertible chars
#define INCONVERTIBLE_PLACEHOLDER '_'


// encoding functions

// encode binary stream and append to string
void appendEncoded(
  const uInt8 *aBinary,
  size_t aSize,
  string &aString,
  TEncodingTypes aEncoding,
  sInt16 aMaxLineSize=76,
  sInt32 aCurrLineSize=0,     // how may chars are on the first line
  bool aSoftBreaksAsCR=false, // if set, soft breaks are not added as CRLF, but only indicated as CR
  bool aEncodeBinary=false    // quoted printable: binary coding: both CR and LF will be
                              // always replaced by "=0D" and "=0A"
);

// decode encoded data and append to string
const char *appendDecoded(
  const char *aText,
  size_t aSize,
  string &aBinString,
  TEncodingTypes aEncoding
);



// generate RFC2822-style address specificiation
// - Common Name will be quoted
// - recipient will be put in angle brackets
void makeRFC2822AddrSpec(
  cAppCharP aCommonName,
  cAppCharP aRecipient,
  string &aRFCAddr
);


// Parse RFC2822-style address specificiation
// - aName will receive name and all (possible) comments
// - aRecipient will receive the (first, in case of a group) email address
cAppCharP parseRFC2822AddrSpec(
  cAppCharP aText,
  string &aName,
  string &aRecipient
);


// RFC2047 encoding

// append internal UTF8 string as RFC2047 style encoding
const char *appendUTF8AsRFC2047(
  const char *aText,
  string &aString
);

// parse character string from RFC2047 style encoding to UTF8 internal string
const char *appendRFC2047AsUTF8(
  const char *aRFC2047,
  stringSize aSize, // max number of chars to look at
  string &aString,
  TLineEndModes aLEM=lem_none
);



// charset conversion functions

// generic bintree-based conversion functions
typedef uInt16 treeval_t;

typedef struct {
  treeval_t minkey;
  treeval_t maxkey;
  treeval_t linksstart;
  treeval_t linksend;
  size_t numelems;
  treeval_t *elements;
} TConvFlatTree;


#ifdef BINTREE_GENERATOR

typedef struct TBinTreeNode {
  treeval_t key;
  struct TBinTreeNode *nextHigher;
  struct TBinTreeNode *nextLowerOrEqual;
  treeval_t value; // valid only if links are both NULL
} TBinTreeNode;


// add a key/value pair to the binary tree
void addToBinTree(TBinTreeNode *&aBinTree, treeval_t aMinKey, treeval_t aMaxKey, treeval_t aKey, treeval_t aValue);
// dispose a bintree
void disposeBinTree(TBinTreeNode *&aBinTree);
// search directly in bintree
treeval_t searchBintree(TBinTreeNode *aBinTree, treeval_t aKey, treeval_t aUndefValue, treeval_t aMinKey, treeval_t aMaxKey);


// make a flat form representation of the bintree in a one-dimensional array
bool flatBinTree(
  TBinTreeNode *aBinTree, TConvFlatTree &aFlatTree, size_t aArrSize,
  treeval_t aMinKey, treeval_t aMaxKey, treeval_t aLinksStart, treeval_t aLinksEnd
);
#endif

// search flattened bintree for a specific key value
treeval_t searchFlatBintree(const TConvFlatTree &aFlatTree, treeval_t aKey, treeval_t aUndefValue);

// add byte char as UTF8 to string value and apply charset translation if needed
//void appendCharAsUTF8(char c, string &aVal, TCharSets aCharSet);
uInt16 appendCharsAsUTF8(const char *aChars, string &aVal, TCharSets aCharSet, uInt16 aNumChars=1);

// add string as UTF8 to value and apply charset translation if needed
// - if aLineEndChar is specified, occurrence of this will be replaced
//   by '\n', occurrence of non matching LF/CR will be ignored
void appendStringAsUTF8(
  const char *s, string &aVal,
  TCharSets aCharSet,
  TLineEndModes aLEM=lem_cstr,
  bool aAllowFilemakerCR=false // if set, 0x0B is interpreted as line end as well
);
// add UTF8 string to value in custom charset
// - aLEM specifies line ends to be used
// - aQuotingMode specifies what quoting (for ODBC literals for example) should be used
// - output is clipped after aMaxBytes bytes (if not 0)
// - returns true if all input could be converted, false if output is clipped
bool appendUTF8ToString(
  cAppCharP aUTF8, string &aVal,
  TCharSets aCharSet,
  TLineEndModes aLEM=lem_none,
  TQuotingModes aQuotingMode=qm_none,
  size_t aMaxBytes=0
);
// same, but output string is cleared first
bool storeUTF8ToString(
  cAppCharP aUTF8, string &aVal,
  TCharSets aCharSet,
  TLineEndModes aLEM=lem_none,
  TQuotingModes aQuotingMode=qm_none,
  size_t aMaxBytes=0
);


// convert UTF8 to UCS4
// - returns pointer to next char
// - returns UCS4=0 on error (no char, bad sequence, sequence not complete)
const char *UTF8toUCS4(const char *aUTF8, uInt32 &aUCS4);
// convert UCS4 to UTF8 (0 char is not allowed and will be ignored!)
void UCS4toUTF8(uInt32 aUCS4, string &aUTF8);


// convert UTF-16 to UCS4
// - returns pointer to next char
// - returns UCS4=0 on error (no char, bad sequence, sequence not complete)
const uInt16 *UTF16toUCS4(const uInt16 *aUTF16P, uInt32 &aUCS4);
// convert UCS4 to UTF-16
// - returns 0 for UNICODE range UCS4 and first word of UTF-16 for non UNICODE
uInt16 UCS4toUTF16(uInt32 aUCS4, uInt16 &aUTF16);



// add UTF8 string as UTF-16 byte stream to 8-bit string
// - if aLEM is not lem_none, occurrence of any type of Linefeeds
//   (LF,CR,CRLF and even CRCRLF) in input string will be
//   replaced by the specified line end type
// - output is clipped after ByteString reaches aMaxBytes size (if not 0), = approx half as many Unicode chars
// - returns true if all input could be converted, false if output is clipped
bool appendUTF8ToUTF16ByteString(
  cAppCharP aUTF8,
  string &aUTF16ByteString,
  bool aBigEndian,
  TLineEndModes aLEM=lem_none,
  uInt32 aMaxBytes=0
);

// add UTF16 byte string as UTF8 to value
void appendUTF16AsUTF8(
  const uInt16 *aUTF16,
  uInt32 aNumUTF16Chars,
  bool aBigEndian,
  string &aVal,
  bool aConvertLineEnds=false,
  bool aAllowFilemakerCR=false
);


// MD5 and B64 given string
void MD5B64(const char *aString, sInt32 aLen, string &aMD5B64);

// format as Timestamp text, usually for logfiles
void StringObjTimestamp(string &aStringObj, lineartime_t aTimer);
// format as hex byte string
void StringObjHexString(string &aStringObj, const uInt8 *aBinary, uInt32 aBinSz);

// add (already encoded!) CGI to existing URL string
bool addCGItoString(string &aStringObj, cAppCharP aCGI, bool noduplicate=true);

// encode string for being used as a CGI key/value element
string encodeForCGI(cAppCharP aCGI);

// Count bits
int countbits(uInt32 aMask);

// make uppercase
void StringUpper(string &aString);
// make lowercase
void StringLower(string &aString);

// Substitute occurences of pattern with replacement in string
void StringSubst(
  string &aString, const char *aPattern, const string &aReplacement,
  sInt32 aPatternLen,
  TCharSets aCharSet, TLineEndModes aLEM,
  TQuotingModes aQuotingMode
);
void StringSubst(
  string &aString, const char *aPattern, const char *aReplacement,
  sInt32 aPatternLen, sInt32 aReplacementLen,
  TCharSets aCharSet=chs_unknown, TLineEndModes aLEM=lem_none,
  TQuotingModes aQuotingMode=qm_none
);
void StringSubst(string &aString, const char *aPattern, const string &aReplacement, sInt32 aPatternLen=-1);
void StringSubst(string &aString, const char *aPattern, sInt32 aNumber, sInt32 aPatternLen=-1);
/* subst regexp
i\=0\; *while\(\(i\=([^.]+)\.find\(\"([^"]+)\",i\)\)\!\=string::npos\) *\{ *[^.]+\.replace\(i,([0-9]+),(.+)\)\; i\+\=.*$
StringSubst(\1,"\2",\4,\3);
*/


// helper macro for allocation of SyncML Toolkit structures from C++ code
#define SML_NEW(ty) ((ty*) _smlMalloc(sizeof(ty)))
#define SML_FREE(m) smlLibFree(m)

// allocate memory via SyncML toolkit allocation function, but throw
// exception if it fails. Used by SML
void *_smlMalloc(MemSize_t size);

// copy PCdata contents into std::string object
void smlPCDataToStringObj(const SmlPcdataPtr_t aPcdataP, string &aStringObj);

// returns pointer to PCdata contents or null string. If aSizeP!=NULL, length will be stored in *aSize
const char *smlPCDataToCharP(const SmlPcdataPtr_t aPcdata, stringSize *aSizeP=NULL);

// returns pointer to PCdata contents if existing, NULL otherwise.
// If aSizeP!=NULL, length will be stored in *aSize
const char *smlPCDataOptToCharP(const SmlPcdataPtr_t aPcdataP, stringSize *aSizeP=NULL);

// returns item string or empty string (NEVER NULL)
const char *smlItemDataToCharP(const SmlItemPtr_t aItemP);

// returns first item string or empty string (NEVER NULL)
const char *smlFirstItemDataToCharP(const SmlItemListPtr_t aItemListP);

// split Hostname into address and port parts
void splitHostname(const char *aHost,string *aAddr,string *aPort);

// split URL into protocol, hostname, document name and auth-info (user, password);
// none of the strings are url-decoded, do that as needed
void splitURL(const char *aURI,string *aProtocol,string *aHost,string *aDoc,string *aUser, string *aPasswd,
              string *aPort, string *aQuery);

// in-place decoding of %XX, NULL pointer allowed
void urlDecode(string *str);

// returns error code made ready for SyncML sending (that is, remove offset
// of 10000 if present, and make generic error 500 for non-SyncML errors,
// and return LOCERR_OK as 200)
localstatus syncmlError(localstatus aErr);

// returns error code made local (that is, offset by 10000 in case aErr is a
// SyncML status code <10000, and convert 200 into LOCERR_OK)
localstatus localError(localstatus aErr);

// returns pure relative URI, if specified relative or absolute to
// optionally given server URI
const char *relativeURI(const char *aURI,const char *aServerURI=NULL);

// returns pointer to source or target LocURI
const char *smlSrcTargLocURIToCharP(const SmlTargetPtr_t aSrcTargP);

// returns pointer to source or target LocName
const char *smlSrcTargLocNameToCharP(const SmlTargetPtr_t aSrcTargP);

// returns DevInf pointer if any in specified PCData, NULL otherwise
SmlDevInfDevInfPtr_t smlPCDataToDevInfP(const SmlPcdataPtr_t aPCDataP);

// returns MetInf pointer if any in specified PCData, NULL otherwise
SmlMetInfMetInfPtr_t smlPCDataToMetInfP(const SmlPcdataPtr_t aPCDataP);

// returns true on successful conversion of PCData string to format
bool smlPCDataToFormat(const SmlPcdataPtr_t aPCDataP, TFmtTypes &aFmt);


// returns type from meta
const char *smlMetaTypeToCharP(SmlMetInfMetInfPtr_t aMetaP);

// returns Next Anchor from meta
const char *smlMetaNextAnchorToCharP(SmlMetInfMetInfPtr_t aMetaP);

// returns Last Anchor from meta
const char *smlMetaLastAnchorToCharP(SmlMetInfMetInfPtr_t aMetaP);


// build Meta anchor
SmlPcdataPtr_t newMetaAnchor(const char *aNextAnchor, const char *aLastAnchor=NULL);

// build Meta type
SmlPcdataPtr_t newMetaType(const char *aMetaType);

// build empty Meta
SmlPcdataPtr_t newMeta(void);

// copy meta from existing meta (for data items only
// anchor, mem, emi, maxobjsize, nonce are not copied!)
SmlPcdataPtr_t copyMeta(SmlPcdataPtr_t aOldMetaP);


// add an item to an item list
SmlItemListPtr_t *addItemToList(
  SmlItemPtr_t aItemP, // existing item data structure, ownership is passed to list
  SmlItemListPtr_t *aItemListPP // adress of pointer to existing item list or NULL
);

// add a CTData item to a CTDataList
SmlDevInfCTDataListPtr_t *addCTDataToList(
  SmlDevInfCTDataPtr_t aCTDataP, // existing CTData item data structure, ownership is passed to list
  SmlDevInfCTDataListPtr_t *aCTDataListPP // adress of pointer to existing item list or NULL
);

// add a CTDataProp item to a CTDataPropList
SmlDevInfCTDataPropListPtr_t *addCTDataPropToList(
  SmlDevInfCTDataPropPtr_t aCTDataPropP, // existing CTDataProp item data structure, ownership is passed to list
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of pointer to existing item list or NULL
);

// add a CTData describing a property (as returned by newDevInfCTData())
// as a new property without parameters to a CTDataPropList
SmlDevInfCTDataPropListPtr_t *addNewPropToList(
  SmlDevInfCTDataPtr_t aPropCTData, // CTData describing property
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of pointer to existing item list or NULL
);

// add PCData element to a PCData list
SmlPcdataListPtr_t *addPCDataToList(
  SmlPcdataPtr_t aPCDataP, // Existing PCData element to be added, ownership is passed to list
  SmlPcdataListPtr_t *aPCDataListPP // adress of pointer to existing PCData list or NULL
);

// add PCData string to a PCData list
SmlPcdataListPtr_t *addPCDataStringToList(
  const char *aString, // String to be added
  SmlPcdataListPtr_t *aPCDataListPP // adress of pointer to existing PCData list or NULL
);

// create new optional location (source or target)
SmlSourcePtr_t newOptLocation(
  const char *aLocURI,
  const char *aLocName=NULL
);

// create new location (source or target)
SmlSourcePtr_t newLocation(
  const char *aLocURI,
  const char *aLocName=NULL
);

// create new empty Item
SmlItemPtr_t newItem(void);

// create new Item with string-type data
SmlItemPtr_t newStringDataItem(
  const char *aString
);


// create format PCData
SmlPcdataPtr_t newPCDataFormat(
  TFmtTypes aFmtType,
  bool aShowDefault
);

// create new string-type PCData, if NULL or empty string is passed for aData,
// NULL is returned (optional info not there)
SmlPcdataPtr_t newPCDataFormatted(
  const uInt8 *aData,    // data
  sInt32 aLength,         // length of data, if<0 then string length is calculated
  TFmtTypes aEncType,   // encoding
  bool aNeedsOpaque   // set opaque needed (string that could confuse XML parsing or even binary)
);

// create new string-type PCData, if NULL is passed for aString, NULL is returned (optional info not there)
SmlPcdataPtr_t newPCDataOptString(
  const char *aString,
  sInt32 aLength=-1 // length of string, if<0 then length is calculated
);

// create new string-type PCData, if NULL is passed for aString,
// NULL is returned (optional info not there)
// if empty string is passed, PCData with empty contents will be created
SmlPcdataPtr_t newPCDataOptEmptyString(
  const char *aString,
  sInt32 aLength=-1 // length of string, if<0 then length is calculated
);

// create new string-type PCData, if NULL is passed for aString, an empty string is created
SmlPcdataPtr_t newPCDataString(
  const char *aString,
  sInt32 aLength=-1 // length of string, if<0 then length is calculate
);

// create new PCData, aOpaque can be used to generate non-string data
SmlPcdataPtr_t newPCDataStringX(
  const uInt8 *aString,
  bool aOpaque=false, // if set, an opaque method (OPAQUE or CDATA) is used
  sInt32 aLength=-1 // length of string, if<0 then length is calculate
);


// create new string-type PCData from C++ string
SmlPcdataPtr_t newPCDataString(const string &aString);

// create new decimal string representation of long as PCData
SmlPcdataPtr_t newPCDataLong(sInt32 aLong);

// returns true on successful conversion of PCData string to long
bool smlPCDataToLong(const SmlPcdataPtr_t aPCDataP, sInt32 &aLong);
bool smlPCDataToULong(const SmlPcdataPtr_t aPCDataP, uInt32 &aLong);


// create challenge of requested type
SmlChalPtr_t newChallenge(TAuthTypes aAuthType, const string &aNextNonce, bool aBinaryAllowed);

// Nonce generator allowing last-session nonce to be correctly re-generated in next session
void generateNonce(string &aNonce, const char *aDevStaticString, sInt32 aSessionStaticID);

// create new property or param descriptor for CTCap
SmlDevInfCTDataPtr_t newDevInfCTData(cAppCharP aName,uInt32 aSize=0, bool aNoTruncate=false, uInt32 aMaxOccur=0, cAppCharP aDataType=NULL);

// frees prototype element and sets calling pointer to NULL
void FreeProtoElement(void * &aVoidP);
// macro to overcome pointer reference conversion constraints
#ifdef PREFER_MACROS
#define FREEPROTOELEMENT(p) FreeProtoElement((void *&)p)
#else
template <class T> void FREEPROTOELEMENT(T *&p)
{
  smlFreeProtoElement(static_cast<void *>(p));
  p = NULL;
}
#endif

} // namespace sysync

#endif
// eof
