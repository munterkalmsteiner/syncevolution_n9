/*
 *  File:         sysync_utils.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Provides some helper functions interfacing between SyncML Toolkit
 *  and C++
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-16 : luz : created
 *
 */

#include "prefix_file.h"
#include "sync_include.h"
#include "sysync_utils.h"

#include "libmem.h"


#ifdef SYSYNC_TOOL
  #include "syncappbase.h" // for CONSOLEPRINTF
  #include "customimplagent.h" // for DBCharSetNames
#endif

namespace sysync {

// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL

// parse RFC 2822 addr spec
int parse2822AddrSpec(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  addrparse <RFC2822 addr-spec string to parse>"));
    CONSOLEPRINTF(("    Parse name and email address out of a RFC2822-type addr-spec"));
    return EXIT_SUCCESS;
  }
  // check for argument
  if (argc<1) {
    CONSOLEPRINTF(("1 argument required"));
    return EXIT_FAILURE;
  }
  // parse
  string addrname,addremail;
  const char* p=argv[0];
  p=parseRFC2822AddrSpec(p,addrname,addremail);
  // show
  CONSOLEPRINTF(("Input         : %s",argv[0]));
  CONSOLEPRINTF(("Name          : %s",addrname.c_str()));
  CONSOLEPRINTF(("email         : %s",addremail.c_str()));
  CONSOLEPRINTF(("unparsed rest : %s",p));
  return EXIT_SUCCESS;
} // parse2822AddrSpec


// convert between character sets
int charConv(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  charconv [<input charset>] <output charset> <C-string to convert>"));
    CONSOLEPRINTF(("    Convert from one charset to another. Default input is UTF-8"));
    return EXIT_SUCCESS;
  }

  #ifdef __TEST_EQUALITY_OF_CP936_WITH_GB2312__
  // quick test
  uInt32 ch_in;
  for (ch_in=0x8100; ch_in<=0xFFFF; ch_in++) {
    // convert into internal UTF-8
    string s_internal,s_in;
    s_in.erase();
    if (ch_in>=0x8100) s_in+=(ch_in >> 8) & 0xFF;
    s_in+=(ch_in & 0xFF);
    s_internal.erase();
    appendStringAsUTF8(
      s_in.c_str(),
      s_internal,
      chs_gb2312
    );
    // convert into output format
    string s_out;
    s_out.erase();
    appendUTF8ToString(
      s_internal.c_str(),
      s_out,
      chs_cp936
    );
    // show differences
    if (s_in!=s_out && s_out.size()>0 && s_out[0]!=INCONVERTIBLE_PLACEHOLDER) {
      string s1,s2;
      s1.erase(); StrToCStrAppend(s_in.c_str(), s1);
      s2.erase(); StrToCStrAppend(s_out.c_str(), s2);
      CONSOLEPRINTF(("\"%s\" != \"%s\"",s1.c_str(),s2.c_str()));
    }
  }
  return EXIT_SUCCESS;
  #endif

  // check for argument
  if (argc<2) {
    CONSOLEPRINTF(("2 or 3 arguments required"));
    return EXIT_FAILURE;
  }
  int ochsarg=1;
  sInt16 enu;
  // get input charset
  TCharSets charset_in=chs_utf8;
  if (argc==3) {
    // first arg is input charset
    if (!StrToEnum(DBCharSetNames, numCharSets, enu, argv[0])) {
      CONSOLEPRINTF(("'%s' is not a valid input charset name",argv[0]));
      return EXIT_FAILURE;
    }
    charset_in = (TCharSets)enu;
  }
  else {
    ochsarg=0; // first arg ist input charset
  }
  // get output charset
  TCharSets charset_out;
  if (!StrToEnum(DBCharSetNames, numCharSets, enu, argv[ochsarg])) {
    CONSOLEPRINTF(("'%s' is not a valid output charset name",argv[ochsarg]));
    return EXIT_FAILURE;
  }
  charset_out = (TCharSets)enu;
  // get string to convert
  string s_in;
  s_in.erase();
  CStrToStrAppend(argv[ochsarg+1], s_in);
  // convert into internal UTF-8
  string s_internal;
  s_internal.erase();
  appendStringAsUTF8(
    s_in.c_str(),
    s_internal,
    charset_in
  );
  // convert into output format
  string s_out;
  s_out.erase();
  appendUTF8ToString(
    s_internal.c_str(),
    s_out,
    charset_out
  );
  // show all three
  string show;
  // - input
  show.erase(); StrToCStrAppend(s_in.c_str(), show);
  CONSOLEPRINTF(("Input    : %-20s = \"%s\"",DBCharSetNames[charset_in], show.c_str()));
  // - internal UTF8
  show.erase(); StrToCStrAppend(s_internal.c_str(), show);
  CONSOLEPRINTF(("Internal : %-20s = \"%s\"",DBCharSetNames[chs_utf8], show.c_str()));
  // - output
  show.erase(); StrToCStrAppend(s_out.c_str(), show);
  CONSOLEPRINTF(("Output   : %-20s = \"%s\"",DBCharSetNames[charset_out], show.c_str()));
  return EXIT_SUCCESS;
} // charConv

#endif // SYSYNC_TOOL


// conversion table from ANSI 0x80..0x9F to UCS4
static const uInt32 Ansi_80_to_9F_to_UCS4[0x20] = {
  0x20AC,   0  ,0x201A,0x0192, 0x201E,0x2026,0x2020,0x2021, // 0x80..0x87
  0x02C6,0x2030,0x0160,0x2039, 0x0152,   0  ,0x017D,   0  , // 0x88..0x8F
     0  ,0x2018,0x2019,0x201C, 0x201D,0x2022,0x2013,0x2014, // 0x90..0x97
  0x02DC,0x2122,0x0161,0x203A, 0x0153,   0  ,0x017E,0x0178  // 0x98..0x9F
};


// ASCIIfy table to convert umlauts etc. to nearest plain ASCII
typedef struct {
  uInt32 ucs4;
  uInt8 ascii;
} TASCIIfyEntry;

static const TASCIIfyEntry ASCIIfyTable[] = {
  { 0x000000C4, 'A' }, // Adieresis
  { 0x000000C5, 'A' }, // Aring
  { 0x000000C7, 'C' }, // Ccedilla
  { 0x000000C9, 'E' }, // Eacute
  { 0x000000D1, 'N' }, // Ntilde
  { 0x000000D6, 'O' }, // Odieresis
  { 0x000000DC, 'U' }, // Udieresis
  { 0x000000E1, 'a' }, // aacute
  { 0x000000E0, 'a' }, // agrave
  { 0x000000E2, 'a' }, // acircumflex
  { 0x000000E4, 'a' }, // adieresis
  { 0x000000E3, 'a' }, // atilde
  { 0x000000E5, 'a' }, // aring
  { 0x000000E7, 'c' }, // ccedilla
  { 0x000000E9, 'e' }, // eacute
  { 0x000000E8, 'e' }, // egrave
  { 0x000000EA, 'e' }, // ecircumflex
  { 0x000000EB, 'e' }, // edieresis
  { 0x000000ED, 'i' }, // iacute
  { 0x000000EC, 'i' }, // igrave
  { 0x000000EE, 'i' }, // icircumflex
  { 0x000000EF, 'i' }, // idieresis
  { 0x000000F1, 'n' }, // ntilde
  { 0x000000F3, 'o' }, // oacute
  { 0x000000F2, 'o' }, // ograve
  { 0x000000F4, 'o' }, // ocircumflex
  { 0x000000F6, 'o' }, // odieresis
  { 0x000000F5, 'o' }, // otilde
  { 0x000000FA, 'u' }, // uacute
  { 0x000000F9, 'u' }, // ugrave
  { 0x000000FB, 'u' }, // ucircumflex
  { 0x000000FC, 'u' }, // udieresis
  { 0x000000DF, 's' }, // germandoubles
  { 0x000000D8, 'O' }, // Oslash
  { 0x000000F8, 'o' }, // oslash
  { 0x000000C0, 'A' }, // Agrave
  { 0x000000C3, 'A' }, // Atilde
  { 0x000000D5, 'O' }, // Otilde
  { 0x00000152, 'O' }, // OE
  { 0x00000153, 'o' }, // oe
  { 0x000000C6, 'A' }, // AE
  { 0x000000E6, 'a' }, // ae
  { 0x000000C2, 'A' }, // Acircumflex
  { 0x000000CA, 'E' }, // Ecircumflex
  { 0x000000C1, 'A' }, // Aacute
  { 0x000000CB, 'E' }, // Edieresis
  { 0x000000C8, 'E' }, // Egrave
  { 0x000000CD, 'I' }, // Iacute
  { 0x000000CC, 'I' }, // Igrave
  { 0x000000CE, 'i' }, // Icircumflex
  { 0x000000CF, 'i' }, // Odieresis
  { 0x000000D3, 'O' }, // Oacute
  { 0x000000D2, 'O' }, // Ograve
  { 0x000000D4, 'O' }, // Ocircumflex
  // terminator
  { 0,0 }
};


// line end mode names
const char * const lineEndModeNames[numLineEndModes] = {
  "none", // none specified
  "unix", // 0x0A
  "mac",  // 0x0D
  "dos",  // 0x0D 0x0A
  "cstr", // as in C strings, '\n' which is 0x0A normally (but might be 0x0D on some platforms)
  "filemaker" // 0x0B (filemaker tab-separated text format, CR is shown as 0x0B within fields
};



// literal quoting mode names
const char * const quotingModeNames[numQuotingModes] = {
  "none", // none specified
  "singlequote", // single quote must be duplicated
  "doublequote", // double quote must be duplicated
  "backslash" // C-string-style escapes of CR,LF,TAB,BS,\," and ' (but no full c-string escape with \xXX etc.)
};


// Encoding format names for SyncML
const char * const encodingFmtSyncMLNames[numFmtTypes] = {
  "chr",      // plain chars
  "bin",      // binary
  "b64"       // base 64 encoding
};
// Encoding format names for user
const char * const encodingFmtNames[numFmtTypes] = {
  "plain-text",  // no encoding (plain text)
  "binary",      // plain binary (in WBXML only)
  "base64"       // base 64 encoding
};


// field (property) data type names
const char * const propDataTypeNames[numPropDataTypes] = {
  "chr", // Character
  "int", // Integer
  "bool", // Boolean
  "bin", // Binary
  "datetime", // Date and time of day
  "phonenum", // Phone number
  "text", // plain text
  "???" // unknown
};


// Auth type names
const char * const authTypeSyncMLNames[numAuthTypes] = {
  NULL,                   // no authorisation
  "syncml:auth-basic",    // basic (B64 encoded user pw string)
  "syncml:auth-md5"       // Md5 encoded user:pw:nonce
};


// MIME encoding types
const char * const MIMEEncodingNames[numMIMEencodings] = {
  "",
  "7BIT",
  "8BIT",
  "BINARY",
  "QUOTED-PRINTABLE",
  "BASE64",
  "B"
};

// Charset names for MIME based strings
const char * const MIMECharSetNames[numCharSets] = {
  "unknown",
  "US-ASCII",
  "ANSI",
  "ISO-8859-1",
  "UTF-8",
  "UTF-16",
  #ifdef CHINESE_SUPPORT
  "GB2312",
  "CP936",
  #endif
};


#ifdef SYSYNC_ENGINE
// generate RFC2822-style address specificiation
// - Common Name will be quoted
// - recipient will be put in angle brackets
void makeRFC2822AddrSpec(
  cAppCharP aCommonName,
  cAppCharP aRecipient,
  string &aRFCAddr
)
{
  if (aCommonName && *aCommonName) {
    aRFCAddr='"';
    while (*aCommonName) {
      if (*aCommonName=='"') aRFCAddr += "\\\"";
      else aRFCAddr += *aCommonName;
      aCommonName++;
    }
    aRFCAddr+="\" <";
    aRFCAddr+=aRecipient;
    aRFCAddr+=">";
  }
  else {
    // plain email address
    aRFCAddr=aRecipient;
  }
} // makeRFC2822AddrSpec




// sysytool -f syncserv_odbc.xml addrparse "(Lukas Peter) luz@synthesis.ch (Zeller), gaga"

// Parse RFC2822-style address specificiation
// - aName will receive name and all (possible) comments
// - aRecipient will receive the (first, in case of a group) email address
cAppCharP parseRFC2822AddrSpec(
  cAppCharP aText,
  string &aName,
  string &aRecipient
)
{
  const char *p;
  char c;

  enum {
    pstate_sepspace,
    pstate_trailing,
    pstate_text,
    pstate_comment,
    pstate_quoted,
    pstate_email
  } pstate = pstate_trailing;
  string text,groupname;
  bool textcouldbeemail=true;
  bool atfound=false;
  aName.erase();
  aRecipient.erase();
  p=aText;
  do {
    c=*p;
    // check end of input
    if (c==0) break; // done with the string
    // advance to next char
    p++;
    // check according to state
    switch (pstate) {
      case pstate_sepspace:
        if (c==' ') {
          aName+=c;
        }
        pstate=pstate_trailing;
        // otherwise treat like trailing
      case pstate_trailing:
        textcouldbeemail=aRecipient.empty();
        atfound=false;
        // skip trailing WSP first
        if (c==' ' || c=='\t' || c=='\n' || c=='\r') break; // simply ignore WSP in trailing mode
        else pstate=pstate_text;
        // fall trough to do text analysis
      case pstate_text:
        // now check specials
        if (c==',') { c=0; break; } // end of address, cause exit from loop, next will start after comma
        else if (c==';') { c=0; break; } // end of group address list, treat it like single address
        else if (c=='@' && textcouldbeemail) atfound=true; // flag presence of @
        // check if text could still be a email address by itself
        if (textcouldbeemail && !isalnum(c) && c!='@' && c!='_' && c!='-' && c!='.') {
          textcouldbeemail=false;
          if (atfound) {
            aRecipient=text;
            text.erase();
          }
          atfound=false;
        }
        // now check other specials
        if (c=='"') { pstate=pstate_quoted; } // start of quoted string
        else if (c=='(') { pstate=pstate_comment; } // start of comment
        else if (c=='<') { aRecipient.erase(); pstate=pstate_email; } // start of angle-addr, overrides other recipient texts
        else if (c==':') {
          groupname=aRecipient; // what we've probably parsed as recipient
          groupname+=aName; // plus name so far
          groupname+=text; // plus additional text
          text.erase();
          aName.erase();
          aRecipient.erase();
          pstate=pstate_trailing;
        } // flag presence of a group name (which can be used as name if addr itself does not have one)
        else {
          // add other text chars to the text
          text += c;
        }
        break;
      case pstate_quoted:
        if (c=='\\') {
          if (*p) c=*p++; else break; // get next char (if any) and add to result untested
        }
        else if (c=='"') {
          // end of quoted string
          pstate=pstate_sepspace;
          aName+=text;
          text.erase();
          break;
        }
        // add to text
        text += c;
        break;
      case pstate_comment:
        if (c==')') {
          // end of comment
          aName+=text;
          text.erase();
          pstate=pstate_sepspace;
          break;
        }
        // add to text
        text += c;
        break;
      case pstate_email:
        if (!isalnum(c) && c!='@' && c!='_' && c!='-' && c!='.') {
          // any non-email char terminates email, not only '>', but only '>' is swallowed
          if (c!='>') p--; // re-evaluate char in next state
          pstate=pstate_sepspace;
          break;
        }
        // add to email
        aRecipient += c;
        break;
    } // switch
  } while (c!=0);
  // handle case of pure email address without name and without < > brackets or :
  if (aRecipient.empty() && textcouldbeemail && atfound)
    aRecipient = text;
  else
    aName += text;
  // if name is (now) empty, but we have a group name, use the group name
  if (aName.empty()) aName=groupname;
  // remove trailing spaces in aName
  string::size_type n=aName.find_last_not_of(' ');
  if (n!=string::npos) aName.resize(n+1);
  // return where to continue parsing for next addr-spec (if not end of string)
  return p;
} // parseRFC2822AddrSpec



// append internal UTF8 string as RFC2047 style encoding
const char *appendUTF8AsRFC2047(
  const char *aText,
  string &aString
)
{
  const char *p,*q,*r;
  char c;

  p=aText;
  do {
    q=p; // remember start
    // find chars until next char that must be stored as encoded word
    do {
      c=*p;
      if (c==0 || (c & 0x80) || (c=='=' && *(p+1)=='?')) break;
      p++;
    } while(true);
    // copy chars outside encoded word directly
    if (p-q>0) aString.append(q,p-q);
    // check if end of string
    if (c==0) break;
    // pack some chars into encoded word
    // - start word
    aString.append("=?utf-8?B?"); // 10 chars start (+ 2 chars will be added at end)
    // - encoded data must be 75-12=63 chars or less
    //   Using B (=b64) encoding, output of 63 chars = 63/4*3 = max 47 chars.
    //   We use 45 max, as this is evenly divisible by 3 and output is 60 chars
    q=p;
    while (true) {
      // find next space
      while (*q && !isspace(*q) && q-p<45) q++;
      if (q-p>=45) break; // abort if exhausted already
      // find next non-space
      r=q;
      while (isspace(*r)) r++;
      // check if next non-space will start a new word
      if (*r & 0x80) {
        // we should include the next word as well, if possible without exceeding size
        if (r-p<45) {
          q=r;
          continue;
        }
      }
      break;
    }
    // encode binary stream and append to string
    appendEncoded((const uInt8 *)p,q-p,aString,enc_b);
    p=q;
    // - end word
    aString.append("?=");
  } while (true);
  return p;
} // appendUTF8AsRFC2047


// parse character string from RFC2047 style encoding to UTF8 internal string
const char *appendRFC2047AsUTF8(
  const char *aRFC2047,
  stringSize aSize,
  string &aString,
  TLineEndModes aLEM
)
{
  const char *p,*q,*r,*w;
  char c = 0;
  const char *eot = aRFC2047+aSize;

  p=aRFC2047;
  w=NULL; // start of last detected word (to avoid re-scanning)
  while (p<eot) {
    q=p; // remember start
    // find chars until next encoded word
    while (p<eot) {
      c=*p;
      if (c==0 || (p!=w && c=='=' && *(p+1)=='?')) break;
      p++;
    }
    // copy chars outside encoded word directly
    aString.append(q,p-q);
    // check if end of string
    if (p>=eot || c==0) break;
    // try to parse encoded word
    q=p+2;
  scanword:
    // q is now where we start to parse word contents
    // p is where we would re-start reading normally if current word turns out not to be a word at all
    // - remember start of word scan (to avoid re-scanning it)
    w=p;
    // - get charset
    r=q;
    while (q<eot && *q!='?' && isgraph(*q)) q++;
    if (q>=eot || *q!='?') continue; // is not an encoded word, parse normally
    sInt16 en;
    TCharSets charset=chs_unknown;
    if (StrToEnum(MIMECharSetNames, numCharSets, en, r, q-r)) charset=(TCharSets)en;
    // - get encoding
    r=++q; // continue after ? separator
    while (q<eot && *q!='?' && isgraph(*q)) q++;
    if (q>=eot || *q!='?') continue; // is not an encoded word, parse normally
    TEncodingTypes encoding=enc_8bit;
    if (StrToEnum(MIMEEncodingNames, numMIMEencodings, en, r, q-r)) encoding=(TEncodingTypes)en;
    // - get data part
    r=++q;
    while (q+1<eot && *q && *q!=' ' && !(*q=='?' && *(q+1)=='=')) q++;
    if (q>=eot || *q!='?') continue; // is not an encoded word, parse normally
    // - decode
    string decoded;
    appendDecoded(r,q-r,decoded,encoding);
    // - convert to UTF-8
    appendStringAsUTF8(
      decoded.c_str(),
      aString,
      charset,
      aLEM
    );
    // - skip word terminator
    p=q+2;
    // - check for special case of adjacent words
    q=p;
    while (q<eot && isspace(*q)) q++;
    if (q+1<eot && q>p && *q=='=' && *(q+1)=='?') {
      // adjacent encoded words, only separated by space -> ignore space
      // p is after previous word
      q+=2;
      // q is after lead-in of next word
      goto scanword;
    }
    // p is where we continue reading
  }
  return p;
} // appendRFC2047AsUTF8


// decode encoded data and append to string
const char *appendDecoded(
  const char *aText,
  size_t aSize,
  string &aBinString,
  TEncodingTypes aEncoding
)
{
  char c;
  const char *p=aText;
  uInt32 binsz;
  uInt8 *binP;

  switch (aEncoding) {
    case enc_quoted_printable :
      // decode quoted-printable content
      while ((c=*p++)) {
        // char found
        if (c=='=') {
          uInt16 code;
          char hex[2];
          // check for soft break first
          if (*p=='\x0D' || *p=='\x0A') {
            // soft break, swallow
            if (*p=='\x0D') p++;
            if (*p=='\x0A') p++;
            continue;
          }
          // decode
          hex[0]=*p;
          if (*p) {
            p++;
            hex[1]=*p;
            if (*p) {
              p++;
              if (HexStrToUShort(hex,code,2)==2) {
                c=code; // decoded char
              }
              else continue; // simply ignore
            }
            else break;
          }
          else break;
        }
        // append char
        aBinString+=c;
      }
      aText=p;
      break;
    case enc_base64:
    case enc_b:
      // decode base 64
      binsz=0;
      binP = b64::decode(aText, aSize, &binsz);
      aBinString.append((const char *)binP,binsz);
      b64::free(binP);
      aText+=aSize;
      break;
    case enc_7bit:
    case enc_8bit:
      // copy no more than size
      if (aSize>0) aBinString.reserve(aBinString.size()+aSize);
      while (*p && aSize>0) {
        aBinString+=*p++;
        aSize--;
      }
      aText=p;
      break;
    case enc_none:
    case enc_binary:
      // copy bytes
      aBinString.append(aText,aSize);
      aText+=aSize;
      break;
    case numMIMEencodings:
      // invalid
      break;
  } // quoted printable
  return aText;
} // appendDecoded



// encode binary stream and append to string
void appendEncoded(
  const uInt8 *aBinary,
  size_t aSize,
  string &aString,
  TEncodingTypes aEncoding,
  sInt16 aMaxLineSize,
  sInt32 aCurrLineSize,
  bool aSoftBreaksAsCR,
  bool aEncodeBinary
)
{
  char c;
  string::size_type linestart;
  const uInt8 *p;
  bool softbreak;
  uInt32 b64len;
  char *b64;
  bool processed;

  switch (aEncoding) {
    case enc_binary :
    case enc_none :
    case enc_8bit :
    case enc_7bit : // assume we have no 8bit chars
      // just copy 1:1
      aString.append((const char *)aBinary,aSize);
      break;
    case enc_quoted_printable:
      // quote-printable encoding
      // - determine start of last line in aString
      //   Note: this is because property text will be folded when lines aMaxLineSize
      linestart=aString.size()-aCurrLineSize;
      for (p=aBinary;p<aBinary+aSize;p++) { // '\0' will not terminate the 'for' loop
        c=*p;
        if (!aEncodeBinary && !c) break; // still exit at NUL when not encoding real binary data
        processed=false; // input data in c is not yet processed
        // make sure we do not go over the limit (if one is set)
        // - if less than 8 chars (=0D=0A + =\r) are free, soft break the line
        softbreak= aMaxLineSize && (aString.size()-linestart>=string::size_type(aMaxLineSize)-8);
        if (!aEncodeBinary) {
          if (c=='\r') continue;      // ignore them
          if (c=='\b') continue;      // ignore them (optional break indicators, not relevant for QP output)
          if (c=='\n') {              // - encode line ends
            aString.append("=0D=0A"); // special string for Line Ends (CR LF)
            processed = true; // c is processed now
            softbreak = true;
          } // if
        } // if
        // - handle soft line break (but only if really doing line breaking)
        //   Also: avoid adding a soft break at the very end of the string
        if (softbreak && aMaxLineSize && p+1<aBinary+aSize) {
          if (aSoftBreaksAsCR)
            aString.append("=\r");       // '\r' signals softbreak for finalizeproperty()
          else
            aString.append("=\x0D\x0A"); // break line here
          // new line starts after softbreak
          linestart=aString.size();
          // make sure soft line break is not followed by unencoded space
          // (which would look like MIME folding)
          if (c==' ' || (processed && p[1]==' ')) {
            aString.append("=20");
            if (processed) p++; // if current char was already processed, we need to explicitly skip the space
            processed=true; // char is now processed in any case
          } // if
        } // if
        // now encode the char in c if not already processed by now
        if (!processed) {
          bool encodeIt=
            (c=='=')                     // escape equal sign itself
            || (c=='<' && aEncodeBinary) // avoid XML mismatch problems
            || (uInt8)c>0x7F
            || (uInt8)c<0x20;            // '\0' will be encoded as well
          if (encodeIt) { // encode all non ASCII chars > 0x7F (and control chars as well)
            aString+="=";
            aString+=NibbleToHexDigit(c>>4);
            aString+=NibbleToHexDigit(c);
          }
          else
            aString+=c; // just copy
        } // if
      }
      break;
    case enc_base64:
    case enc_b:
      // use base64 encoding
      if (aSize>0) {
        // don't call b64 with size=0!
        b64 = b64::encode(
          aBinary,aSize, // what to encode
          &b64len, // output size
          aMaxLineSize, // max line size
          aSoftBreaksAsCR
        );
        // append to output, if any
        if (b64) {
          aString.append(b64,b64len);
          // release buffer
          b64::free(b64);
        }
        if (aEncoding!=enc_b) {
          // make sure it ends with a newline for "base64" (but NOT for "b" as used in RFC2047)
          // Note: when used in vCard2.1, that newline is part of the property and show as an
          //       empty line in the vCard.
          aString += aSoftBreaksAsCR ? "\r" : "\x0D\x0A";
        }
      }
      break;
    default:
      // do nothing
      break;
  } // switch
} // appendEncoded


#ifdef CHINESE_SUPPORT
// the flatBinTree tables for converting to and from GB2312
#include "gb2312_tables_inc.cpp"
// the flatBinTree tables for converting to and from CP936
#include "cp936_tables_inc.cpp"
#endif


// add char (possibly multi-byte) as UTF8 to value and apply charset translation if needed
// - returns > 0 if aNumChars was not correct number of bytes needed to convert an entire character;
//   return value is number of bytes needed to generate one output character. If return value
//   is<>0, no char has been appended to aVal.
uInt16 appendCharsAsUTF8(const char *aChars, string &aVal, TCharSets aCharSet, uInt16 aNumChars)
{
  uInt32 ucs4;
  // first char
  uInt8 c=*aChars;
  // this is a 8-bit char
  switch(aCharSet) {
    case chs_utf8 :
      // UTF8 is native charset of the application, simply add
      aVal+=c;
      break;
    case chs_ansi :
    case chs_iso_8859_1 :
      // do poor man's conversion to UCS4
      // - most ANSI chars are 1:1 mapped
      ucs4 = ((uInt8)c & 0xFF);
      // - except 0x80..0x9F, use table for these
      if (ucs4>=0x80 && ucs4<=0x9F)
        ucs4=Ansi_80_to_9F_to_UCS4[ucs4-0x80];
      // - convert to UTF8
      UCS4toUTF8(ucs4,aVal);
      break;
    #ifdef CHINESE_SUPPORT
    case chs_gb2312 : // simplified Chinese GB-2312 charset
      // all below 0x80 are passed as-is
      if (c<0x80)
        aVal+=c; // simply append
      else {
        // 16-bit GB2312 char
        if (aNumChars!=2)
          return 2; // we need 2 chars for a successful GB-2312
        // we have 2 bytes, convert them
        ucs4 = searchFlatBintree(gb2312_to_ucs2, (c<<8) + (uInt8)aChars[1], INCONVERTIBLE_PLACEHOLDER);
        // - convert to UTF8
        UCS4toUTF8(ucs4,aVal);
      }
      break;
    case chs_cp936: // simplified chinese Windows codepage CP936
      if (c<0x80)
        aVal+=c; // simply append
      else {
        // 0x0080 (euro sign) or 2-byte CP936
        if (c==0x80)
          ucs4=searchFlatBintree(cp936_to_ucs2, 0x0080, INCONVERTIBLE_PLACEHOLDER);
        else {
          // 16-bit GB2312 char
          if (aNumChars!=2)
            return 2; // we need 2 chars for a successful CP936
          // we have 2 bytes, convert them
          ucs4 = searchFlatBintree(cp936_to_ucs2, (c<<8) + (uInt8)aChars[1], INCONVERTIBLE_PLACEHOLDER);
        }
        // - convert to UTF8
        UCS4toUTF8(ucs4,aVal);
      }
      break;
    #endif
    case chs_ascii : // plain 7-bit ASCII
    default : // unknown
      // only 7-bit allowed
      if (c & 0x80)
        aVal+=INCONVERTIBLE_PLACEHOLDER;
      else
        aVal+=c;
      break;
  } // switch
  return 0; // ok, converted aNumChars
} // appendCharsAsUTF8




// add string as UTF8 to value and apply charset translation if needed
// - if lineEndMode is not lem_none, all sorts of line ends will be converted
//   to the specified mode.
void appendStringAsUTF8(const char *s, string &aVal, TCharSets aCharSet, TLineEndModes aLEM, bool aAllowFilemakerCR)
{
  char c;
  const char *start=s;
  if (s) {
    while ((c=*s++)!=0) {
      if (aLEM!=lem_none) {
        // line end handling enabled
        if (c==0x0D) {
          // could be mac (0x0D) or DOS (0x0D/0x0A)
          if (*s==0x0A) {
            // this is DOS-type line end
            // - consume the 0x0A as well
            s++;
            // - check for 0x0D 0x0D 0x0A special case (caused by
            //   DOS-text-file conversion of non-DOS strings)
            if (s>=start+3) {
              if (*(s-3)==0x0D) {
                // char before the DOS-CRLF was a 0x0D as well (and
                // has already produced a newline in the output
                // --> completely ignore this CRLF
                continue;
              }
            }
          }
          // is a line end, convert it to platform-lineend
          c='\n'; // platform
        }
        else if (c==0x0A) {
          // 0x0A without preceeding 0x0D = unix
          c='\n'; // platform
        }
        else if (c==0x0B && aAllowFilemakerCR) {
          // 0x0B is used as lineend in filemaker export and achilformat
          c='\n';
        }
        // line end converted to platform
        if (c=='\n' && aLEM!=lem_cstr) {
          // produce specified line end
          switch (aLEM) {
            case lem_mac : c=0x0D; break;
            case lem_unix : c=0x0A; break;
            case lem_filemaker : c=0x0B; break;
            case lem_dos :
              c=0x0A; // LF will be added later
              aVal+=0x0D; // add CR
              break;
            default: break;
          }
        }
      } // line end handling enabled
      // normal add
      uInt16 i,seqlen=1; // assume logical char consists of single byte
      do {
        seqlen=appendCharsAsUTF8(s-seqlen,aVal,aCharSet,seqlen); // add char (possibly with UTF8 expansion) to aVal
        if (seqlen<=1) break; // done
        for (i=1;i<seqlen;i++) { if (*s==0) break; else s++; }
        if (i<seqlen) break; // not enough bytes
      } while(true);
    }
  }
} // appendStringAsUTF8



// same as appendUTF8ToString, but output string is cleared first
bool storeUTF8ToString(
  cAppCharP aUTF8, string &aVal,
  TCharSets aCharSet,
  TLineEndModes aLEM,
  TQuotingModes aQuotingMode,
  size_t aMaxBytes
)
{
  aVal.erase();
  return appendUTF8ToString(aUTF8,aVal,aCharSet,aLEM,aQuotingMode,aMaxBytes);
} // storeUTF8ToString



// helper for adding chars
static void appendCharToString(
  char c,
  string &aVal,
  TQuotingModes aQuotingMode
) {
  if (aQuotingMode==qm_none) {
    aVal+=c;
  }
  else if (aQuotingMode==qm_backslash) {
    // treat CR, LF, BS, TAB, single/doublequote and backslash specially
    if (c==0x0D)
      aVal+="\\r";
    else if (c==0x0A)
      aVal+="\\n";
    else if (c==0x08)
      aVal+="\\b";
    else if (c==0x09)
      aVal+="\\t";
    else if (c=='"')
      aVal+="\\\"";
    else if (c=='\'')
      aVal+="\\'";
    else if (c=='\\')
      aVal+="\\\\";
    else
      aVal+=c;
  }
  else if (aQuotingMode==qm_duplsingle) {
    if (c=='\'') aVal+=c; // duplicate
    aVal+=c; // normal append
  }
  else if (aQuotingMode==qm_dupldouble) {
    if (c=='"') aVal+=c; // duplicate
    aVal+=c; // normal append
  }
} // appendCharToString


// add UTF8 string to value in custom charset
// - if aLEM is not lem_none, occurrence of any type of Linefeeds
//   (LF,CR,CRLF and even CRCRLF) in input string will be
//   replaced by the specified line end type
// - aQuotingMode specifies what quoting (for ODBC literals for example) should be used
// - output is clipped after aMaxBytes bytes (if not 0)
// - returns true if all input could be converted, false if output is clipped
bool appendUTF8ToString(
  cAppCharP aUTF8,
  string &aVal,
  TCharSets aCharSet,
  TLineEndModes aLEM,
  TQuotingModes aQuotingMode,
  size_t aMaxBytes
)
{
  uInt32 ucs4;
  uInt8 c;
  size_t n=0;
  cAppCharP p=aUTF8;
  cAppCharP start=aUTF8;

  if (!aUTF8) return true; // nothing to copy, copied everything of that!
  if (aCharSet==chs_utf8 && aLEM==lem_none && aQuotingMode==qm_none) {
    // shortcut: simply append entire string
    if (aMaxBytes==0)
      aVal+=aUTF8;
    else
      aVal.append(aUTF8,aMaxBytes);
    // advance "processed" pointer behind consumed part of string
    p=aUTF8+aVal.size();
  }
  else {
    // process char by char
    while((c=*aUTF8)!=0 && (aMaxBytes==0 || n<aMaxBytes)) {
      p=aUTF8;
      // check for linefeed conversion
      if (aLEM!=lem_none && (c==0x0D || c==0x0A)) {
        aUTF8++;
        // line end, handling enabled
        if (c==0x0D) {
          // could be mac (0x0D) or DOS (0x0D/0x0A)
          if (*aUTF8==0x0A) {
            // this is DOS-type line end
            // - consume the 0x0A as well
            aUTF8++;
            // - check for 0x0D 0x0D 0x0A special case (caused by
            //   DOS-text-file conversion of non-DOS strings)
            if (aUTF8>=start+3) {
              if (*(aUTF8-3)==0x0D) {
                // char before the DOS-CRLF was a 0x0D as well (and
                // has already produced a newline in the output
                // --> completely ignore this CRLF
                continue;
              }
            }
          }
          // is a line end, convert it to platform-lineend
          c='\n'; // platform
        }
        else { // must be 0x0A
          // 0x0A without preceeding 0x0D = unix
          c='\n'; // platform
        }
        // line end converted to platform
        if (aLEM!=lem_cstr) {
          // produce specified line end
          switch (aLEM) {
            case lem_mac : c=0x0D; break;
            case lem_filemaker : c=0x0B; break;
            case lem_unix : c=0x0A; break;
            case lem_dos :
              c=0x0A; // LF will be added later
              n++; // count it extra
              if (aMaxBytes && n>=aMaxBytes)
                goto stringfull; // no room to complete it, ignore it
              appendCharToString(0x0D,aVal,aQuotingMode);
              break;
            default: break;
          }
        }
        appendCharToString(c,aVal,aQuotingMode);
        n++; // count it
      } // line end, handling enabled
      else {
        // non lineend (or lineend not handled specially)
        if (aCharSet==chs_utf8) {
          aUTF8++;
          // - simply add char
          appendCharToString(c,aVal,aQuotingMode);
          n++;
        }
        else {
          // - make UCS4
          p=aUTF8; // save previous position to detect if we have processed all
          aUTF8=UTF8toUCS4(aUTF8,ucs4);
          // now we have UCS4
          if (ucs4==0) {
            // UTF8 resulting in UCS4 null char is not allowed
            ucs4=INCONVERTIBLE_PLACEHOLDER;
          }
          else {
            // convert to specified charset
            switch (aCharSet) {
              case chs_ansi:
              case chs_iso_8859_1:
                if ((ucs4<=0xFF && ucs4>=0xA0) || ucs4<0x80)
                  // 00..7F and A0..FF directly map to ANSI
                  appendCharToString(ucs4,aVal,aQuotingMode);
                else {
                  // search for matching ANSI in table
                  uInt8 k;
                  for (k=0; k<0x20; k++) {
                    if (ucs4==Ansi_80_to_9F_to_UCS4[k]) {
                      // found in table
                      break;
                    }
                  }
                  if (k<0x20)
                    // conversion found
                    aVal+=k+0x80;
                  else
                    // no conversion found in table
                    aVal+=INCONVERTIBLE_PLACEHOLDER;
                } // not in 1:1 range 0..7F, A0..FF
                n++;
                break;
              #ifdef CHINESE_SUPPORT
              case chs_gb2312 : // simplified Chinese GB-2312 charset
                // all below 0x80 are passed as-is
                if (ucs4<0x80) {
                  appendCharToString(ucs4,aVal,aQuotingMode); // simply append ASCII codes
                  n++;
                }
                else {
                  // convert to 16-bit GB2312 char
                  uInt16 gb = searchFlatBintree(ucs2_to_gb2312, ucs4, INCONVERTIBLE_PLACEHOLDER);
                  // check if we have space
                  if (aMaxBytes!=0 && n+2>aMaxBytes)
                    goto stringfull;
                  // append as two bytes to output string
                  aVal+=gb >> 8;
                  aVal+=gb & 0xFF;
                  n+=2;
                }
                break;
              case chs_cp936 : // simplified Chinese CP936 windows codepage
                // all below 0x80 are passed as-is
                if (ucs4<0x80) {
                  appendCharToString(ucs4,aVal,aQuotingMode); // simply append ASCII codes
                  n++;
                }
                else {
                  // convert to CP936 16-bit representation
                  uInt16 twobytes = searchFlatBintree(ucs2_to_cp936, ucs4, INCONVERTIBLE_PLACEHOLDER);
                  // append as two bytes to output string, but only this is a CP936 two-byte at all
                  if (twobytes>0x0080) {
                    // check if we have space
                    if (aMaxBytes!=0 && n+2>aMaxBytes)
                      goto stringfull;
                    aVal+=twobytes >> 8; // sub-page lead in
                    n++;
                  }
                  aVal+=twobytes & 0xFF; // sub-page code
                  n++;
                }
                break;
              #endif
              case chs_ascii:
                // explicit ASCII: convert some special chars to plain ASCII
                if ((ucs4 & 0xFFFFFF80) !=0) {
                  // search in ASCIIfy table
                  uInt16 k=0;
                  while (ASCIIfyTable[k].ucs4!=0) {
                    if (ucs4==ASCIIfyTable[k].ucs4) {
                      // found, fetch ASCII-equivalent
                      ucs4=ASCIIfyTable[k].ascii;
                      break; // use it
                    }
                    k++;
                  }
                }
                // fall through to default, which does not know ANY non-ASCII
              default:
                // only 7 bit ASCII is allowed
                if ((ucs4 & 0xFFFFFF80) !=0)
                  aVal+=INCONVERTIBLE_PLACEHOLDER;
                else
                  appendCharToString(ucs4,aVal,aQuotingMode); // simply append ASCII codes
                n++;
                break;
            } // switch
          } // valid UCS4
        } // not already UTF8
      } // if not lineend
      // processed until here
      p=aUTF8;
    } // while not end of input string
  } // not already UTF8
  // return true if input string completely consumed
stringfull:
  return (*p==0);
} // appendUTF8ToString


// convert UTF8 to UCS4
// - returns pointer to next char
// - returns UCS4=0 on error (no char, bad sequence, sequence not complete)
const char *UTF8toUCS4(const char *aUTF8, uInt32 &aUCS4)
{
  uInt8 c;
  sInt16 morechars;

  if ((c=*aUTF8)!=0) {
    aUTF8++;
    // there is a char
    morechars=0;
    // decode UTF8 lead-in
    if ((c & 0x80) == 0) {
      // single byte
      aUCS4=c;
      morechars=0;
    }
    else if ((c & 0xE0) == 0xC0) {
      // two bytes
      aUCS4=c & 0x1F;
      morechars=1;
    }
    else if ((c & 0xF0) == 0xE0) {
      aUCS4=c & 0x0F;
      morechars=2;
    }
    else if ((c & 0xF8) == 0xF0) {
      aUCS4=c & 0x07;
      morechars=3;
    }
    else if ((c & 0xFC) == 0xF8) {
      aUCS4=c & 0x03;
      morechars=4;
    }
    else if ((c & 0xFE) == 0xFC) {
      aUCS4=c & 0x01;
      morechars=5;
    }
    else {
      // bad char
      aUCS4=0;
    }
    // process additional chars
    while(morechars--) {
      if ((c=*aUTF8)==0) {
        // unfinished sequence
        aUCS4=0;
        break;
      }
      aUTF8++;
      if ((c & 0xC0) != 0x80) {
        // bad additional char
        aUCS4=0;
        break;
      }
      // each additional char adds 6 new bits
      aUCS4 = aUCS4 << 6; // shift existing bits
      aUCS4 |= (c & 0x3F); // add new bits
    }
  }
  else {
    // no char
    aUCS4=0;
  }
  // return pointer to next char
  return aUTF8;
} // UTF8toUCS4


// convert UCS4 to UTF8 (0 char is not allowed and will be ignored!)
void UCS4toUTF8(uInt32 aUCS4, string &aUTF8)
{
  uInt8 c;

  // ignore null char
  if (aUCS4==0) return;
  // create UTF8 lead-in
  sInt16 morechars=0;
  if (aUCS4<0x00000080) {
    // one byte
    c=aUCS4;
  }
  else if (aUCS4<0x00000800) {
    // two bytes
    c=0xC0 | ((aUCS4 >> 6) & 0x1F);
    morechars=1;
  }
  else if (aUCS4<0x00010000) {
    // three bytes
    c=0xE0 | ((aUCS4 >> 12) & 0x0F);
    morechars=2;
  }
  else if (aUCS4<0x00200000) {
    // four bytes
    c=0xF0 | ((aUCS4 >> 18) & 0x07);
    morechars=3;
  }
  else if (aUCS4<0x04000000) {
    // five bytes
    c=0xF8 | ((aUCS4 >> 24) & 0x03);
    morechars=4;
  }
  else {
    // six bytes
    c=0xFC | ((aUCS4 >> 30) & 0x01);
    morechars=5;
  }
  // add lead-in
  aUTF8+=c;
  // add rest of sequence
  while (morechars--) {
    c= 0x80 | ((aUCS4 >> (morechars * 6)) & 0x3F);
    aUTF8+=c;
  }
} // UCS4toUTF8


/* Encoding UTF-16 (excerpt from RFC 2781, paragraph 2.1)

   Encoding of a single character from an ISO 10646 character value to
   UTF-16 proceeds as follows. Let U be the character number, no greater
   than 0x10FFFF.

   1) If U < 0x10000, encode U as a 16-bit unsigned integer and
      terminate.

   2) Let U' = U - 0x10000. Because U is less than or equal to 0x10FFFF,
      U' must be less than or equal to 0xFFFFF. That is, U' can be
      represented in 20 bits.

   3) Initialize two 16-bit unsigned integers, W1 and W2, to 0xD800 and
      0xDC00, respectively. These integers each have 10 bits free to
      encode the character value, for a total of 20 bits.

   4) Assign the 10 high-order bits of the 20-bit U' to the 10 low-order
      bits of W1 and the 10 low-order bits of U' to the 10 low-order
      bits of W2. Terminate.

   Graphically, steps 2 through 4 look like:
   U' = yyyyyyyyyyxxxxxxxxxx
   W1 = 110110yyyyyyyyyy
   W2 = 110111xxxxxxxxxx
*/

// convert UCS4 to UTF-16
// - returns 0 for UNICODE range UCS4 and first word of UTF-16 for non UNICODE
uInt16 UCS4toUTF16(uInt32 aUCS4, uInt16 &aUTF16)
{
  if (aUCS4<0x10000) {
    // in unicode range: single UNICODE char
    aUTF16=aUCS4;
    return 0; // no second char
  }
  else {
    // out of UNICODE range
    aUCS4-=0x10000;
    if (aUCS4>0xFFFF) {
      // inconvertible
      aUTF16=INCONVERTIBLE_PLACEHOLDER;
      return 0;
    }
    else {
      // convert to two-word UNICODE / UCS-2
      aUTF16=0xD800+(aUCS4>>10);
      return 0xDC00+(aUCS4 & 0x03FF);
    }
  }
} // UCS4toUTF16



/* Decoding UTF-16

   Decoding of a single character from UTF-16 to an ISO 10646 character
   value proceeds as follows. Let W1 be the next 16-bit integer in the
   sequence of integers representing the text. Let W2 be the (eventual)
   next integer following W1.

   1) If W1 < 0xD800 or W1 > 0xDFFF, the character value U is the value
      of W1. Terminate.

   2) Determine if W1 is between 0xD800 and 0xDBFF. If not, the sequence
      is in error and no valid character can be obtained using W1.
      Terminate.

   3) If there is no W2 (that is, the sequence ends with W1), or if W2
      is not between 0xDC00 and 0xDFFF, the sequence is in error.
      Terminate.

   4) Construct a 20-bit unsigned integer U', taking the 10 low-order
      bits of W1 as its 10 high-order bits and the 10 low-order bits of
      W2 as its 10 low-order bits.

   5) Add 0x10000 to U' to obtain the character value U. Terminate.

   Note that steps 2 and 3 indicate errors. Error recovery is not
   specified by this document. When terminating with an error in steps 2
   and 3, it may be wise to set U to the value of W1 to help the caller
   diagnose the error and not lose information. Also note that a string
   decoding algorithm, as opposed to the single-character decoding
   described above, need not terminate upon detection of an error, if
   proper error reporting and/or recovery is provided.

*/

// convert UTF-16 to UCS4
// - returns pointer to next char
// - returns UCS4=0 on error (no char, bad sequence, sequence not complete)
const uInt16 *UTF16toUCS4(const uInt16 *aUTF16P, uInt32 &aUCS4)
{
  uInt16 utf16=*aUTF16P++;

  if (utf16<0xD800 || utf16>0xDFFF) {
    // single char unicode
    aUCS4=utf16;
  }
  else {
    // could be two-char
    if (utf16<=0xDBFF) {
      // valid first char: check second char
      uInt16 utf16_2 = *aUTF16P; // next
      if (utf16_2 && utf16_2>=0xDC00 && utf16_2<=0xDFFF) {
        // second char exists and is valid
        aUTF16P++; // advance now
        aUCS4 =
          ((utf16 & 0x3FF) << 10) +
          (utf16_2 & 0x3FF);
      }
      else
        aUCS4=0; // no char
    }
    else {
      aUCS4=0; // no char
    }
  }
  // return advanced pointer
  return aUTF16P;
} // UCS4toUTF16






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
  TLineEndModes aLEM,
  uInt32 aMaxBytes
)
{
  uInt32 ucs4;
  uInt16 utf16=0,utf16_1;
  cAppCharP p;

  while (aUTF8 && *aUTF8) {
    // convert next UTF8 char to UCS4
    p=UTF8toUCS4(aUTF8, ucs4);
    if (ucs4==0) break; // error in UTF8 encoding, exit
    // convert line ends
    if (ucs4 == '\n' && aLEM!=lem_none && aLEM!=lem_cstr) {
      // produce specified line end
      utf16_1=0;
      switch (aLEM) {
        case lem_mac : utf16=0x0D; break;
        case lem_filemaker : utf16=0x0B; break;
        case lem_unix : utf16=0x0A; break;
        case lem_dos :
          utf16_1=0x0D; // CR..
          utf16=0x0A; // ..then LF
          break;
        default: break;
      }
    }
    else {
      // ordinary char, use UTF16 encoding
      utf16_1 = UCS4toUTF16(ucs4,utf16);
    }
    // check if appending UTF16 would exceed max size specified
    if (aMaxBytes!=0 && aUTF16ByteString.size() + (utf16_1 ? 4 : 2) > aMaxBytes)
      break;
    // we can append, advance input pointer
    aUTF8 = p;
    // now append
    if (aBigEndian) {
      // Big end first, Motorola order
      if (utf16_1) {
        aUTF16ByteString += (char)((utf16_1 >> 8) & 0xFF);
        aUTF16ByteString += (char)(utf16_1 & 0xFF);
      }
      aUTF16ByteString += (char)((utf16 >> 8) & 0xFF);
      aUTF16ByteString += (char)(utf16 & 0xFF);
    }
    else {
      // Little end first, Intel order
      if (utf16_1) {
        aUTF16ByteString += (char)((utf16_1 >> 8) & 0xFF);
        aUTF16ByteString += (char)(utf16_1 & 0xFF);
      }
      aUTF16ByteString += (char)(utf16 & 0xFF);
      aUTF16ByteString += (char)((utf16 >> 8) & 0xFF);
    }
  } // while
  // true if all input consumed
  return (aUTF8==NULL) || (*aUTF8==0);
} // appendUTF8ToUTF16ByteString


// add UTF16 byte string as UTF8 to value
void appendUTF16AsUTF8(
  const uInt16 *aUTF16,
  uInt32 aNumUTF16Chars,
  bool aBigEndian,
  string &aVal,
  bool aConvertLineEnds,
  bool aAllowFilemakerCR
)
{
  uInt32 ucs4;
  uInt16 utf16pair[2];
  cAppCharP inP = (cAppCharP)aUTF16;
  bool lastWasCR=false;

  while (inP && !(*inP==0 && *(inP+1)==0) && aNumUTF16Chars>0) {
    // get two words (in case of surrogate pair)
    if (aBigEndian) {
      // Motorola order
      utf16pair[0]=((*(inP) & 0xFF)<<8) + (*(inP+1) & 0xFF);
      if (aNumUTF16Chars>1) utf16pair[1]=((*(inP+2) & 0xFF)<<8) + (*(inP+3) & 0xFF);
    }
    else {
      // Intel order
      utf16pair[0]=((*(inP+1) & 0xFF)<<8) + (*(inP) & 0xFF);
      if (aNumUTF16Chars>1) utf16pair[1]=((*(inP+3) & 0xFF)<<8) + (*(inP+2) & 0xFF);
    }
    cAppCharP hP = (cAppCharP)UTF16toUCS4(utf16pair, ucs4);
    /*
    PDEBUGPRINTFX(DBG_PARSE+DBG_EXOTIC,(
      "Parsed %ld bytes: *(inP)=0x%02hX, *(inP+1)=0x%02hX, *(inP+2)=0x%02hX, *(inP+3)=0x%02hX, utf16pair[0]=0x%04hX, utf16pair[1]=0x%04hX, ucs4=0x%04lX",
      (uInt32)(hP-(cAppCharP)utf16pair),
      (uInt16)*(inP), (uInt16)*(inP+1), (uInt16)*(inP+2), (uInt16)*(inP+3),
      (uInt16)utf16pair[0], (uInt16)utf16pair[1],
      (uInt32)ucs4
    ));
    */
    uInt32 bytes=hP-(cAppCharP)utf16pair;
    inP+=bytes; // next UTF16 to check
    aNumUTF16Chars-=bytes/2; // count down UTF16 chars
    // convert line ends if selected
    if (aConvertLineEnds) {
      if (ucs4 == 0x0D) {
        lastWasCR=true;
        continue;
      }
      else {
        if (ucs4 == 0x0A || (aAllowFilemakerCR && ucs4 == 0x0B))
          ucs4 = '\n'; // convert to LineEnd
        else if (lastWasCR)
          aVal += '\n'; // insert a LineEnd
        lastWasCR=false;
      }
    }
    // append to UTF-8 string
    UCS4toUTF8(ucs4, aVal);
  }
  if (lastWasCR)
    aVal += '\n'; // input string ended on CR, must be shown in output
} // appendUTF16AsUTF8






#ifdef BINTREE_GENERATOR

// add a key/value pair to the binary tree
void addToBinTree(TBinTreeNode *&aBinTree, treeval_t aMinKey, treeval_t aMaxKey, treeval_t aKey, treeval_t aValue)
{
  // start at root
  TBinTreeNode **nextPP = &aBinTree;
  treeval_t cmpval;
  do {
    // create the new decision value from max and min
    cmpval = aMinKey+((aMaxKey-aMinKey) >> 1);
    // create the node if not already there
    if (*nextPP==NULL) {
      *nextPP = new TBinTreeNode;
      (*nextPP)->key = cmpval;
      (*nextPP)->nextHigher=NULL;
      (*nextPP)->nextLowerOrEqual=NULL;
      (*nextPP)->value=0;
    }
    // check if the node CREATED is a leaf node
    // this is the case if max==min
    if (aMaxKey==aMinKey) {
      // save leaf value (possibly overwriting existing leaf value for same code)
      (*nextPP)->value=aValue;
      break;
    }
    // decide which way to go
    if (aKey>cmpval) {
      // go to the "higher" side
      nextPP = &((*nextPP)->nextHigher);
      // determine new minimum
      aMinKey = cmpval+1; // minimum must be higher than cmpval
    }
    else {
      // go to the "lower or equal" side
      nextPP = &((*nextPP)->nextLowerOrEqual);
      // determine new maximum
      aMaxKey = cmpval; // maximum must be lower or equal than cmpval
    }
  } while(true);
} // addToBinTree


// dispose a bintree
void disposeBinTree(TBinTreeNode *&aBinTree)
{
  if (!aBinTree) return;
  if (aBinTree->nextHigher)
    disposeBinTree(aBinTree->nextHigher);
  if (aBinTree->nextLowerOrEqual)
    disposeBinTree(aBinTree->nextLowerOrEqual);
  delete aBinTree;
  aBinTree=NULL;
} // disposeBinTree


// convert key to value using a flat bintree
treeval_t searchBintree(TBinTreeNode *aBinTree, treeval_t aKey, treeval_t aUndefValue, treeval_t aMinKey, treeval_t aMaxKey)
{
  treeval_t cmpval;
  while(aBinTree) {
    // create the new decision value from max and min
    cmpval = aMinKey+((aMaxKey-aMinKey) >> 1);
    // must match stored cmpval
    if (cmpval!=aBinTree->key)
      return aUndefValue;
    // check if next node must be leaf if the tree contains our key,
    // this is the case if max==min
    if (aMaxKey==aMinKey) {
      if (aBinTree->nextHigher!=NULL || aBinTree->nextLowerOrEqual!=NULL) {
        // no leaf value here, should not be the case ever (we should have
        // encountered a node with no left or right link before this!)
        return aUndefValue;
      }
      else {
        // found a leaf value here
        return aBinTree->value;
      }
    }
    // decide which way to go
    if (aKey>cmpval) {
      // go to the "higher" side = just next element in array, except if we have the special marker here
      if (aBinTree->nextHigher == NULL)
        return aUndefValue; // we should go higher-side, but can't -> unknown key
      aBinTree=aBinTree->nextHigher;
      // determine new minimum
      aMinKey = cmpval+1; // minimum must be higher than cmpval
    }
    else {
      // go to the "lower" side = element at index indicated by current element, except if we have the special marker here
      if (aBinTree->nextLowerOrEqual == NULL)
        return aUndefValue; // we should go lower-or-equal-side, but can't -> unknown key
      aBinTree=aBinTree->nextLowerOrEqual;
      // determine new maximum
      aMaxKey = cmpval; // maximum must be lower or equal than cmpval
    }
  }
  // if we reach the end of the array, key is not in the tree
  return aUndefValue;
} // searchBintree




// make a flat form representation of the bintree in a one-dimensional array
// - higher-side links are implicit (nodes following each other),
//   lower-or-equal-side links are explicit
static bool flatBinTreeRecursion(
  TBinTreeNode *aBinTree, size_t &aIndex, treeval_t *aFlatArray, size_t aArrSize, treeval_t aLinksStart, treeval_t aLinksEnd
)
{
  // check if array is full
  if (aIndex>=aArrSize)
    return false;
  // examine node to flatten
  if (aBinTree->nextHigher==NULL && aBinTree->nextLowerOrEqual==NULL) {
    // this is a leaf node, containing only the value
    if (aBinTree->value>=aLinksStart && aBinTree->value<=aLinksEnd)
      return false; // link space and value space overlap
    aFlatArray[aIndex]=aBinTree->value;
    aIndex++;
  }
  else if (aBinTree->nextHigher==NULL) {
    // lower-side-only node: set special mark to specify that lower-or-equal side
    // implicitly follows (instead of higher-side)
    aFlatArray[aIndex]=aLinksStart + 1; // no node points to the immediately following node explicitly, so 1 can be used as special marker
    aIndex++;
    // - recurse to generate it
    if (!flatBinTreeRecursion(aBinTree->nextLowerOrEqual,aIndex,aFlatArray,aArrSize,aLinksStart,aLinksEnd))
      return false;
  }
  else {
    // this is a branch
    // - lower-or-equal side is represented as an index in the array
    aFlatArray[aIndex]=aLinksStart + 0; // default to not-existing (no node points to itself, so 0 can be used as NIL index value)
    // - higher side branch follows immediately
    size_t linkindex = aIndex++;
    // - recurse to generate it
    if (!flatBinTreeRecursion(aBinTree->nextHigher,aIndex,aFlatArray,aArrSize,aLinksStart,aLinksEnd))
      return false;
    // - now we have the index where we must insert the lower-or-equal side
    if (aBinTree->nextLowerOrEqual!=NULL) {
      // there is a lower-or-equal side
      // - place relative link from original node
      uInt32 rellink=aIndex-linkindex;
      if ((uInt32)aLinksStart+rellink>(uInt32)aLinksEnd-1L) {
        // we need a long link
        // - move generated higher side branch one up
        for (size_t k=aIndex-1; k>linkindex; k--) aFlatArray[k+1]=aFlatArray[k];
        aIndex++; // we've eaten up one extra entry now
        // - now set long link
        aFlatArray[linkindex]=aLinksEnd-1; // long link marker
        if (rellink>0xFFFF)
          return false; // cannot jump more than 64k
        aFlatArray[linkindex+1]=rellink; // long link
      }
      else {
        // short link is ok
        aFlatArray[linkindex]=aLinksStart+rellink;
      }
      // - now create the lower-or-equal side
      if (!flatBinTreeRecursion(aBinTree->nextLowerOrEqual,aIndex,aFlatArray,aArrSize,aLinksStart,aLinksEnd))
        return false;
    }
  }
  return true;
} // flatBinTreeRecursion


// make a flat form representation of the bintree in a one-dimensional array
// - higher-side links are implicit (nodes following each other),
//   lower-or-equal-side links are explicit
bool flatBinTree(
  TBinTreeNode *aBinTree, TConvFlatTree &aFlatTree, size_t aArrSize,
  treeval_t aMinKey, treeval_t aMaxKey, treeval_t aLinksStart, treeval_t aLinksEnd
)
{
  // save tree params
  aFlatTree.numelems=0;
  aFlatTree.minkey=aMinKey;
  aFlatTree.maxkey=aMaxKey;
  aFlatTree.linksstart=aLinksStart;
  aFlatTree.linksend=aLinksEnd;
  // now create actual tree
  size_t index=0;
  if (!flatBinTreeRecursion(aBinTree,index,aFlatTree.elements,aArrSize,aLinksStart,aLinksEnd))
    return false;
  aFlatTree.numelems=index; // actual length of array
  return true;
} // flatBinTree




#endif


// convert key to value using a flat bintree
treeval_t searchFlatBintree(const TConvFlatTree &aFlatTree, treeval_t aKey, treeval_t aUndefValue)
{
  treeval_t cmpval,thisnode;
  size_t index=0;
  // get start min and max
  treeval_t minKey = aFlatTree.minkey;
  treeval_t maxKey = aFlatTree.maxkey;
  // reject out-of-bounds keys immediately
  if (aKey<minKey || aKey>maxKey)
    return aUndefValue;
  do {
    // create the new decision value from max and min
    cmpval = minKey+((maxKey-minKey) >> 1);
    thisnode = aFlatTree.elements[index];
    // check if next node must be leaf if the tree contains our key,
    // this is the case if max==min
    if (maxKey==minKey) {
      #ifdef BINTREE_GENERATOR
      if (thisnode>=aFlatTree.linksstart && thisnode<=aFlatTree.linksend) {
        // no leaf value here, should not be the case ever (we should have
        // encountered a node with no left or right link before this!)
        return aUndefValue;
      }
      else
      #endif
      {
        // found a leaf value here
        return (treeval_t) thisnode;
      }
    }
    // decide which way to go
    if (aKey>cmpval) {
      // go to the "higher" side = just next element in array, except if we have the special marker here
      if (thisnode == aFlatTree.linksstart+1)
        return aUndefValue; // we should go higher-side, but can't -> unknown key
      // next node is next index (or one more in case this is a long link)
      if (thisnode == aFlatTree.linksend-1)
        index++;
      index++;
      // determine new minimum
      minKey = cmpval+1; // minimum must be higher than cmpval
    }
    else {
      // go to the "lower" side = element at index indicated by current element, except if we have the special marker here
      if (thisnode == aFlatTree.linksstart+1)
        index++; // special case, "lower" side is immediately following because there is no "higher" side
      else {
        #ifdef BINTREE_GENERATOR
        // if node contains a leaf value instead of a link, something is wrong
        if (thisnode<aFlatTree.linksstart || thisnode>aFlatTree.linksend)
          return aUndefValue; // no leaf expected here
        #endif
        if (thisnode==aFlatTree.linksend-1) {
          // long link
          index++; // skip long link marker
          thisnode = aFlatTree.elements[index]; // get link value
          index = index+thisnode; // jump by link value
        }
        else {
          // short link
          index = index+(thisnode-aFlatTree.linksstart); // get index of next node (relative branch)
        }
        if (index==0)
          return aUndefValue; // there is no link
      }
      // determine new maximum
      maxKey = cmpval; // maximum must be lower or equal than cmpval
    }
  } while(index<aFlatTree.numelems);
  // if we reach the end of the array, key is not in the tree
  return aUndefValue;
} // searchFlatBintree

// MD5 and B64 given string
void MD5B64(const char *aString, sInt32 aLen, string &aMD5B64)
{
  // determine input length
  if (aLen<=0) aLen=strlen(aString);
  // calc MD5
  md5::SYSYNC_MD5_CTX context;
  uInt8 digest[16];
  md5::Init (&context);
  md5::Update (&context, (const uInt8 *)aString,aLen);
  md5::Final (digest, &context);
  // b64 encode the MD5 digest
  uInt32 b64md5len;
  char *b64md5=b64::encode(digest,16,&b64md5len);
  // assign result
  aMD5B64.assign(b64md5,b64md5len);
  // done
  b64::free(b64md5); // return buffer allocated by b64::encode
} // MD5B64


// format as Timestamp for use in debug logs
void StringObjTimestamp(string &aStringObj, lineartime_t aTimer)
{
  // format the time
  if (aTimer==noLinearTime) {
    aStringObj = "<no time>";
    return;
  }
  sInt16 y,mo,d,h,mi,s,ms;
  lineartime2date(aTimer,&y,&mo,&d);
  lineartime2time(aTimer,&h,&mi,&s,&ms);
  StringObjPrintf(
    aStringObj,
    "%04d-%02d-%02d %02d:%02d:%02d.%03d",
    y,mo,d,h,mi,s,ms
  );
} // StringObjTimestamp


// format as hex string
void StringObjHexString(string &aStringObj, const uInt8 *aBinary, uInt32 aBinSz)
{
  aStringObj.erase();
  if (!aBinary) return;
  while (aBinSz>0) {
    AppendHexByte(aStringObj,*aBinary++);
    aBinSz--;
  }
} // StringObjHexString


// add (already encoded!) CGI to existing URL string
bool addCGItoString(string &aStringObj, cAppCharP aCGI, bool noduplicate)
{
  if (!noduplicate || aStringObj.find(aCGI)==string::npos) {
    // - Add CGI separator if and only if none exists already
    if (aStringObj.find("?")==string::npos)
      aStringObj += '?';
    aStringObj += aCGI;
    return true; // added
  }
  return false; // nothing added
}


// encode string for being used as a CGI key/value element
string encodeForCGI(cAppCharP aCGI)
{
  string cgi;
  cAppCharP p = aCGI;
  while (p && *p) {
    if (*p>0x7E || *p<=0x20 || *p=='%' || *p=='?' || *p=='&' || *p=='#') {
      // CGI encode these
      cgi += '%';
      AppendHexByte(cgi, *p);
    }
    else {
      // use as-is
      cgi += *p;
    }
    p++;
  }
  return cgi;
} // encodeForCGI


// Count bits
int countbits(uInt32 aMask)
{
  int bits=0;
  uInt32 mask=0x0000001;
  while (mask) {
    if (aMask & mask) bits++;
    mask=mask << 1;
  }
  return bits;
} // countbits


// make uppercase
void StringUpper(string &aString)
{
  for(uInt32 k=0; k<aString.size(); k++) aString[k]=toupper(aString[k]);
} // StringUpper


// make lowercase
void StringLower(string &aString)
{
  for(uInt32 k=0; k<aString.size(); k++) aString[k]=tolower(aString[k]);
} // StringLower


// Substitute occurences of pattern with replacement in string
void StringSubst(
  string &aString, const char *aPattern, const string &aReplacement,
  sInt32 aPatternLen,
  TCharSets aCharSet, TLineEndModes aLEM,
  TQuotingModes aQuotingMode
)
{
  StringSubst(
    aString, aPattern,
    aReplacement.c_str(),
    aPatternLen,
    aReplacement.size(),
    aCharSet, aLEM, aQuotingMode
  );
} // StringSubst


// Substitute occurences of pattern with replacement in string
void StringSubst(
  string &aString, const char *aPattern, const char *aReplacement,
  sInt32 aPatternLen, sInt32 aReplacementLen,
  TCharSets aCharSet, TLineEndModes aLEM,
  TQuotingModes aQuotingMode
)
{
  string::size_type i;
  string s;
  i=0;
  if (aPatternLen<0) aPatternLen=strlen(aPattern);
  // convert if needed
  if (!aReplacement) {
    aReplacement=""; // empty string if not specified
    aReplacementLen=0;
  }
  if (aCharSet!=chs_unknown) {
    appendUTF8ToString(aReplacement,s,aCharSet,aLEM,aQuotingMode);
    aReplacement=s.c_str();
    aReplacementLen=s.size();
  }
  else {
    if (aReplacementLen<0) aReplacementLen=strlen(aReplacement);
  }
  // now replace
  while((i=aString.find(aPattern,i))!=string::npos) {
    aString.replace(i,aPatternLen,aReplacement);
    i+=aReplacementLen;
  }
} // StringSubst


// Substitute occurences of pattern with replacement in string
void StringSubst(string &aString, const char *aPattern, const string &aReplacement, sInt32 aPatternLen)
{
  StringSubst(aString,aPattern,aReplacement.c_str(),aPatternLen,aReplacement.size());
} // StringSubst


// Substitute occurences of pattern with integer number in string
void StringSubst(string &aString, const char *aPattern, sInt32 aNumber, sInt32 aPatternLen)
{
  string s;
  StringObjPrintf(s,"%ld",(long)aNumber);
  StringSubst(aString,aPattern,s,aPatternLen);
} // StringSubst



// copy PCdata contents into std::string object
void smlPCDataToStringObj(const SmlPcdataPtr_t aPcdataP, string &aStringObj)
{
  if (!aPcdataP || !aPcdataP->content) {
    // no content at all
    aStringObj.erase();
  }
  else if (
    // NOTE: Opaque works only with modified syncML toolkit which
    //       makes sure opaque content is ALSO TERMINATED LIKE A C-STRING
    aPcdataP->contentType == SML_PCDATA_STRING ||
    aPcdataP->contentType == SML_PCDATA_OPAQUE
  ) {
    // string or opaque type
    aStringObj.assign((char *)aPcdataP->content, aPcdataP->length);
  }
  else if (aPcdataP->contentType == SML_PCDATA_EXTENSION) {
    // extension type
    StringObjPrintf(aStringObj,"[PCDATA_EXTENSION Type=%hd]",(sInt16)aPcdataP->extension);
  }
  else {
    // other type
    StringObjPrintf(aStringObj,"[PCDATA Type=%hd]",(sInt16)aPcdataP->contentType);
  }
} // smlPCDataToStringObj


// returns item string or empty string (NEVER NULL)
const char *smlItemDataToCharP(const SmlItemPtr_t aItemP)
{
  if (!aItemP) return "";
  return smlPCDataToCharP(aItemP->data);
} // smlItemDataToCharP


// returns first item string or empty string (NEVER NULL)
const char *smlFirstItemDataToCharP(const SmlItemListPtr_t aItemListP)
{
  if (!aItemListP) return "";
  return smlItemDataToCharP(aItemListP->item);
} // smlFirstItemDataToCharP
#endif //SYSYNC_ENGINE

// returns pointer to PCdata contents or null string. If aSizeP!=NULL, length will be stored in *aSize
const char *smlPCDataToCharP(const SmlPcdataPtr_t aPcdataP, stringSize *aSizeP)
{
  const char *str = smlPCDataOptToCharP(aPcdataP, aSizeP);
  if (str) return str;
  return "";
} // smlPCDataToCharP


// returns pointer to PCdata contents if existing, NULL otherwise.
// If aSizeP!=NULL, length will be stored in *aSize
const char *smlPCDataOptToCharP(const SmlPcdataPtr_t aPcdataP, stringSize *aSizeP)
{
  if (!aPcdataP || !aPcdataP->content) {
    return NULL; // we have no value, it could be empty howevert
    if (aSizeP) *aSizeP=0;
  }
  if (aPcdataP->length==0) {
    // empty content
    if (aSizeP) *aSizeP=0;
    return ""; // return empty string
  }
  else if (
    // NOTE: Opaque works only with modified syncML toolkit which
    //       makes sure opaque content is ALSO TERMINATED LIKE A C-STRING
    aPcdataP->contentType == SML_PCDATA_STRING ||
    aPcdataP->contentType == SML_PCDATA_CDATA || // XML only
    aPcdataP->contentType == SML_PCDATA_OPAQUE // WBXML only
  ) {
    // return pointer to content
    if (aSizeP) *aSizeP=aPcdataP->length;
    return (char *) aPcdataP->content;
  }
  else {
    // no string
    if (aSizeP) *aSizeP=11;
    return "[no string]";
  }
} // smlPCDataOptToCharP


// returns pointer to source or target LocURI
const char *smlSrcTargLocURIToCharP(const SmlTargetPtr_t aSrcTargP)
{
  if (!aSrcTargP || !aSrcTargP->locURI) {
    return ""; // empty string
  }
  else {
    // return PCdata string contents
    return smlPCDataToCharP(aSrcTargP->locURI);
  }
} // smlSrcTargLocURIToCharP


// returns pointer to source or target LocName
const char *smlSrcTargLocNameToCharP(const SmlTargetPtr_t aSrcTargP)
{
  if (!aSrcTargP || !aSrcTargP->locName) {
    return ""; // empty string
  }
  else {
    // return PCdata string contents
    return smlPCDataToCharP(aSrcTargP->locName);
  }
} // smlSrcTargLocNameToCharP


#ifdef SYSYNC_ENGINE
// returns error code made ready for SyncML sending (that is, remove offset
// of 10000 if present, and make generic error 500 for non-SyncML errors,
// and return LOCERR_OK as 200)
localstatus syncmlError(localstatus aErr)
{
  if (aErr==LOCERR_OK) return 200; // SyncML ok code
  if (aErr<999) return aErr; // return as is
  if (aErr>=LOCAL_STATUS_CODE+100 && aErr<=999)
    return aErr-LOCAL_STATUS_CODE; // return with offset removed
  // no suitable conversion
  return 500; // return generic "bad"
} // localError


// returns error code made local (that is, offset by 10000 in case aErr is a
// SyncML status code <10000, and convert 200 into LOCERR_OK)
localstatus localError(localstatus aErr)
{
  if (aErr==200 || aErr==0) return LOCERR_OK;
  if (aErr<LOCAL_STATUS_CODE) return aErr+LOCAL_STATUS_CODE;
  return aErr;
} // localError


// returns pure relative URI, if specified relative or absolute to
// given server URI
const char *relativeURI(const char *aURI,const char *aServerURI)
{
  // check for "./" type relative URI
  if (strnncmp(aURI,URI_RELPREFIX,2)==0) {
    // relative URI prefixed with "./", just zap the relative part
    return aURI+2;
  }
  else if (aServerURI) {
    // test if absolute URI specifying the right server
    uInt32 n=strlen(aServerURI);
    if (strnncmp(aURI,aServerURI,n)==0) {
      // beginning of URI matches server's URI
      const char *p=aURI+n;
      // skip delimiter, if any
      if (*p=='/') p++;
      // return relative part of URI
      return p;
    }
  }
  // just return unmodified
  return aURI;
} // relativeURI


// split Hostname into address and port parts
void splitHostname(const char *aHost,string *aAddr,string *aPort)
{
  const char *p,*q;
  p=aHost;
  q=strchr(p,':');
  if (q) {
    // port spec found
    if (aAddr) aAddr->assign(p,q-p);
    if (aPort) aPort->assign(q+1);
  }
  else {
    // no prot spec
    if (aAddr) aAddr->assign(p);
    if (aPort) aPort->erase();
  }
} // splitHostname

// translate %XX into corresponding character in-place
void urlDecode(string *str)
{
  // nothing todo?
  if (!str ||
      str->find('%') == string::npos) return;

  string replacement;
  replacement.reserve(str->size());
  const char *in = str->c_str();
  char c;
  while ((c = *in++) != 0) {
    if (c == '%') {
      c = tolower(*in++);
      unsigned char value = 0;
      if (!c) {
          break;
      } else if (c >= '0' && c <= '9') {
        value = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        value = c - 'a' + 10;
      } else {
        // silently skip invalid character
      }
      value *= 16;
      c = tolower(*in++);
      if (!c) {
        break;
      } else if (c >= '0' && c <= '9') {
        value += c - '0';
        replacement.append((char *)&value, 1);
      } else if (c >= 'a' && c <= 'f') {
        value += c - 'a' + 10;
        replacement.append((char *)&value, 1);
      } else {
        // silently skip invalid character
      }
    } else {
      replacement.append(&c, 1);
    }
  }
  *str = replacement;
}

// split URL into protocol, hostname, document name and auth-info (user, password);
// the optional query and port are not url-decoded, everything else is
void splitURL(const char *aURI,string *aProtocol,string *aHost, 
              string *aDoc, string *aUser, string *aPasswd,
              string *aPort, string *aQuery)
{
  const char *p,*q,*r;

  p=aURI;
  // extract protocol
  q=strchr(p,':');
  if (q) {
    // protocol found
    if (aProtocol) aProtocol->assign(p,q-p);
    p=q+1; // past colon
    int count = 0;
    while (*p=='/' && count < 2) {
      p++; // past trailing slashes (two expected, ignore if less are given)
      count++;
    }
    // now identify end of host part
    string host;
    q=strchr(p, '/');
    if (!q) {
      // no slash, skip forward to end of string
      q = p + strlen(p);
    }
    host.assign(p, q - p);

    // if protocol specified, check for auth info
    const char *h = host.c_str();
    q=strchr(h,'@');
    r=strchr(h,':');
    if (q && r && q>r) {
      // auth exists
      if (aUser) aUser->assign(h,r-h);
      if (aPasswd) aPasswd->assign(r+1,q-r-1);
      // skip auth in full string
      p += q + 1 - h;
    }
    else {
      // no auth found
      if (aUser) aUser->erase();
      if (aPasswd) aPasswd->erase();
    }
    // p now points to host part, as expected below
  }
  else {
    // no protocol found
    if (aProtocol) aProtocol->erase();
    // no protocol, no auth
    if (aUser) aUser->erase();
    if (aPasswd) aPasswd->erase();
  }
  // separate hostname and document
  std::string host;
  // - check for path
  q=strchr(p,'/');
  // - if no path, check if there is a CGI param directly after the host name
  if (!q) {
    // doc part left empty in this case
    if (aDoc) aDoc->erase();
    q=strchr(p,'?');
    if (q) {
      // query directly follows host
      host.assign(p, q - p);
      if (aQuery) aQuery->assign(q + 1);
    } else {
      // entire string is considered the host
      host.assign(p);
      if (aQuery) aQuery->erase();
    }
  }
  else {
    // host part stops at slash
    host.assign(p, q - p);
    // in case of '/', do not put slash into docname
    // even if it would be empty (caller expected to add
    // slash as needed)
    p = q + 1; // exclude slash
    // now check for query
    q=strchr(p,'?');
    if (q) {
      // split at question mark
      if (aDoc) aDoc->assign(p, q - p);
      if (aQuery) aQuery->assign(q + 1);
    } else {
      // whole string is document name
      if (aDoc) aDoc->assign(p);
      if (aQuery) aQuery->erase();
    }
  }

  // remove optional port from host part before url-decoding, because
  // that might introduce new : characters into the host name
  size_t colon = host.find(':');
  if (colon != host.npos) {
    if (aHost) aHost->assign(host.substr(0, colon));
    if (aPort) aPort->assign(host.substr(colon + 1));
  } else {
    if (aHost) aHost->assign(host);
    if (aPort) aPort->erase();
  }
} // splitURL

#ifdef SPLIT_URL_MAIN

#include <stdio.h>
#include <assert.h>

static void test(const std::string &in, const std::string &expected)
{
  string protocol, host, doc, user, password, port, query;
  char buffer[1024];

  splitURL(in.c_str(), &protocol, &host, &doc, &user, &password, &port, &query);

  // URL-decode each part
  urlDecode(&protocol);
  urlDecode(&host);
  urlDecode(&doc);
  urlDecode(&user);
  urlDecode(&password);

  sprintf(buffer,
          "prot '%s' user '%s' passwd '%s' host '%s' port '%s' doc '%s' query '%s'",
          protocol.c_str(),
          user.c_str(),
          password.c_str(),
          host.c_str(),
          port.c_str(),
          doc.c_str(),
          query.c_str());
  printf("%s -> %s\n", in.c_str(), buffer);
  assert(expected == buffer);
}

int main(int argc, char **argv)
{
  test("http://user:passwd@host/patha/pathb?query",
       "prot 'http' user 'user' passwd 'passwd' host 'host' port '' doc 'patha/pathb' query 'query'");
  test("http://user:passwd@host:port/patha/pathb?query",
       "prot 'http' user 'user' passwd 'passwd' host 'host' port 'port' doc 'patha/pathb' query 'query'");
  test("file:///foo/bar",
       "prot 'file' user '' passwd '' host '' port '' doc 'foo/bar' query ''");
  test("http://host%3a:port?param=value",
       "prot 'http' user '' passwd '' host 'host:' port 'port' doc '' query 'param=value'");
  test("http://host%3a?param=value",
       "prot 'http' user '' passwd '' host 'host:' port '' doc '' query 'param=value'");
  test("foo%24",
       "prot '' user '' passwd '' host 'foo$' port '' doc '' query ''");
  test("foo%2f",
       "prot '' user '' passwd '' host 'foo/' port '' doc '' query ''");
  test("foo%2A",
       "prot '' user '' passwd '' host 'foo*' port '' doc '' query ''");
  test("foo%24bar",
       "prot '' user '' passwd '' host 'foo$bar' port '' doc '' query ''");
  test("%24bar",
       "prot '' user '' passwd '' host '$bar' port '' doc '' query ''");
  test("foo%2",
       "prot '' user '' passwd '' host 'foo' port '' doc '' query ''");
  test("foo%",
       "prot '' user '' passwd '' host 'foo' port '' doc '' query ''");
  test("foo%g",
         "prot '' user '' passwd '' host 'foo' port '' doc '' query ''");
  test("foo%gh",
       "prot '' user '' passwd '' host 'foo' port '' doc '' query ''");
  test("%ghbar",
       "prot '' user '' passwd '' host 'bar' port '' doc '' query ''");
  return 0;
}
#endif // SPLIT_URL_MAIN

#endif //SYSYNC_ENGINE


// returns type from meta
const char *smlMetaTypeToCharP(SmlMetInfMetInfPtr_t aMetaP)
{
  if (!aMetaP) return NULL; // no meta at all
  return smlPCDataToCharP(aMetaP->type);
} // smlMetaTypeToCharP



// returns Next Anchor from meta
const char *smlMetaNextAnchorToCharP(SmlMetInfMetInfPtr_t aMetaP)
{
  if (!aMetaP) return NULL; // no meta at all
  if (!aMetaP->anchor) return NULL; // no anchor at all
  return smlPCDataToCharP(aMetaP->anchor->next);
} // smlMetaAnchorToCharP


// returns Last Anchor from meta
const char *smlMetaLastAnchorToCharP(SmlMetInfMetInfPtr_t aMetaP)
{
  if (!aMetaP) return NULL; // no meta at all
  if (!aMetaP->anchor) return NULL; // no anchor at all
  return smlPCDataToCharP(aMetaP->anchor->last);
} // smlMetaLastAnchorToCharP


// returns DevInf pointer if any in specified PCData, NULL otherwise
SmlDevInfDevInfPtr_t smlPCDataToDevInfP(const SmlPcdataPtr_t aPCDataP)
{
  if (!aPCDataP) return NULL;
  if (aPCDataP->contentType!=SML_PCDATA_EXTENSION) return NULL;
  if (aPCDataP->extension!=SML_EXT_DEVINF) return NULL;
  return (SmlDevInfDevInfPtr_t)(aPCDataP->content);
} // smlPCDataToDevInfP


// returns MetInf pointer if any in specified PCData, NULL otherwise
SmlMetInfMetInfPtr_t smlPCDataToMetInfP(const SmlPcdataPtr_t aPCDataP)
{
  if (!aPCDataP) return NULL;
  if (aPCDataP->contentType!=SML_PCDATA_EXTENSION) return NULL;
  if (aPCDataP->extension!=SML_EXT_METINF) return NULL;
  return (SmlMetInfMetInfPtr_t)(aPCDataP->content);
} // smlPCDataToMetInfP


// allocate memory via SyncML toolkit allocation function, but throw
// exception if it fails. Used by SML
void *_smlMalloc(MemSize_t size)
{
  void *p;

  p=smlLibMalloc(size);
  if (!p) SYSYNC_THROW(TMemException("smlLibMalloc() failed"));
  return p;
} // _smlMalloc


// returns true on successful conversion of PCData string to sInt32
bool smlPCDataToULong(const SmlPcdataPtr_t aPCDataP, uInt32 &aLong)
{
  return StrToULong(smlPCDataToCharP(aPCDataP),aLong);
} // smlPCDataToLong

// returns true on successful conversion of PCData string to sInt32
bool smlPCDataToLong(const SmlPcdataPtr_t aPCDataP, sInt32 &aLong)
{
  return StrToLong(smlPCDataToCharP(aPCDataP),aLong);
} // smlPCDataToLong

#ifdef SYSYNC_ENGINE
// returns true on successful conversion of PCData string to format
bool smlPCDataToFormat(const SmlPcdataPtr_t aPCDataP, TFmtTypes &aFmt)
{
  const char *fmt = smlPCDataToCharP(aPCDataP);
  sInt16 sh;
  if (*fmt) {
    if (!StrToEnum(encodingFmtSyncMLNames,numFmtTypes,sh,fmt))
      return false; // unknown format
    aFmt=(TFmtTypes)sh;
  }
  else {
    aFmt=fmt_chr; // no spec = chr
  }
  return true;
} // smlPCDataToFormat
#endif //SYSYNC_ENGINE

// build Meta anchor
SmlPcdataPtr_t newMetaAnchor(const char *aNextAnchor, const char *aLastAnchor)
{
  SmlPcdataPtr_t metaP;
  SmlMetInfAnchorPtr_t anchorP;

  // - create empty meta
  metaP=newMeta();
  // - create new anchor
  anchorP=SML_NEW(SmlMetInfAnchor_t);
  // - set anchor contents
//%%%  anchorP->last=newPCDataOptEmptyString(aLastAnchor); // optional, but omitted only if string is NULL (not if only empty)
  anchorP->last=newPCDataOptString(aLastAnchor); // optional
  anchorP->next=newPCDataString(aNextAnchor); // mandatory
  // - set anchor
  ((SmlMetInfMetInfPtr_t)(metaP->content))->anchor=anchorP;
  // return
  return metaP;
} // newMetaAnchor


// build Meta type
SmlPcdataPtr_t newMetaType(const char *aMetaType)
{
  SmlPcdataPtr_t metaP;

  // - if not type, we don't create a meta at all
  if (aMetaType==NULL || *aMetaType==0) return NULL;
  // - create empty meta
  metaP=newMeta();
  // - set type
  ((SmlMetInfMetInfPtr_t)(metaP->content))->type=newPCDataString(aMetaType);
  // return
  return metaP;
} // newMetaType


// build empty Meta
SmlPcdataPtr_t newMeta(void)
{
  SmlPcdataPtr_t metaP;
  SmlMetInfMetInfPtr_t metinfP;

  // - create empty PCData
  metaP = SML_NEW(SmlPcdata_t);
  metaP->contentType=SML_PCDATA_EXTENSION;
  metaP->extension=SML_EXT_METINF;
  // - %%% assume length is not relevant for structured content (looks like in mgrutil.c)
  metaP->length=0;
  // - create empty meta
  metinfP = SML_NEW(SmlMetInfMetInf_t);
  metaP->content=metinfP; // link to PCdata
  // - init meta options
  metinfP->version=NULL;
  metinfP->format=NULL;
  metinfP->type=NULL;
  metinfP->mark=NULL;
  metinfP->size=NULL;
  metinfP->nextnonce=NULL;
  metinfP->maxmsgsize=NULL;
  metinfP->mem=NULL;
  metinfP->emi=NULL; // PCData list
  metinfP->anchor=NULL;
  // - SyncML 1.1
  metinfP->maxobjsize=NULL;
  // - SyncML 1.2
  metinfP->flags=0;
  // return
  return metaP;
} // newMeta


// copy meta from existing meta (for data items only
// anchor, mem, emi, nonce are not copied!)
// Note however that we copy maxobjsize, as we (mis-)use it for ZIPPED_BINDATA_SUPPORT
SmlPcdataPtr_t copyMeta(SmlPcdataPtr_t aOldMetaP)
{
  if (!aOldMetaP) return NULL;
  SmlPcdataPtr_t newmetaP=newMeta();
  if (!newmetaP) return NULL;
  SmlMetInfMetInfPtr_t oldmetinfP = smlPCDataToMetInfP(aOldMetaP);
  if (!oldmetinfP) return NULL;
  SmlMetInfMetInfPtr_t newmetInfP = smlPCDataToMetInfP(newmetaP);
  // - copy meta
  newmetInfP->version = smlPcdataDup(oldmetinfP->version);
  newmetInfP->format = smlPcdataDup(oldmetinfP->format);
  newmetInfP->type = smlPcdataDup(oldmetinfP->type);
  newmetInfP->mark = smlPcdataDup(oldmetinfP->mark);
  newmetInfP->size = smlPcdataDup(oldmetinfP->size);
  newmetInfP->maxobjsize = smlPcdataDup(oldmetinfP->maxobjsize);
  // return
  return newmetaP;
} // copyMeta




// add an item to an item list
SmlItemListPtr_t *addItemToList(
  SmlItemPtr_t aItemP, // existing item data structure, ownership is passed to list
  SmlItemListPtr_t *aItemListPP // adress of pointer to existing item list or NULL
)
{
  if (aItemListPP && aItemP) {
    // find last itemlist pointer
    while (*aItemListPP) {
      aItemListPP=&((*aItemListPP)->next);
    }
    // aItemListPP now points to a NULL pointer which must be replaced by addr of new ItemList entry
    *aItemListPP = SML_NEW(SmlItemList_t);
    (*aItemListPP)->next=NULL;
    (*aItemListPP)->item=aItemP; // insert new item
    // return pointer to pointer to next element (which is now NULL).
    // Can be passed in to addPCDataToList() again to append more elements without searching
    // for end-of-list
    return &((*aItemListPP)->next);
  }
  // nop, return pointer unmodified
  return aItemListPP;
} // addItemToList


// add a CTData item to a CTDataList
SmlDevInfCTDataListPtr_t *addCTDataToList(
  SmlDevInfCTDataPtr_t aCTDataP, // existing CTData item data structure, ownership is passed to list
  SmlDevInfCTDataListPtr_t *aCTDataListPP // adress of pointer to existing item list or NULL
)
{
  if (aCTDataListPP && aCTDataP) {
    // find last itemlist pointer
    while (*aCTDataListPP) {
      aCTDataListPP=&((*aCTDataListPP)->next);
    }
    // aItemListPP now points to a NULL pointer which must be replaced by addr of new ItemList entry
    *aCTDataListPP = SML_NEW(SmlDevInfCTDataList_t);
    (*aCTDataListPP)->next=NULL;
    (*aCTDataListPP)->data=aCTDataP; // insert new data
    // return pointer to pointer to next element (which is now NULL).
    // Can be passed in to addPCDataToList() again to append more elements without searching
    // for end-of-list
    return &((*aCTDataListPP)->next);
  }
  // nop, return pointer unmodified
  return aCTDataListPP;
} // addCTDataToList


// add a CTDataProp item to a CTDataPropList
SmlDevInfCTDataPropListPtr_t *addCTDataPropToList(
  SmlDevInfCTDataPropPtr_t aCTDataPropP, // existing CTDataProp item data structure, ownership is passed to list
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of pointer to existing item list or NULL
)
{
  if (aCTDataPropListPP && aCTDataPropP) {
    // find last itemlist pointer
    while (*aCTDataPropListPP) {
      aCTDataPropListPP=&((*aCTDataPropListPP)->next);
    }
    // aItemListPP now points to a NULL pointer which must be replaced by addr of new ItemList entry
    *aCTDataPropListPP = SML_NEW(SmlDevInfCTDataPropList_t);
    (*aCTDataPropListPP)->next=NULL;
    (*aCTDataPropListPP)->data=aCTDataPropP; // insert new data
    // return pointer to pointer to next element (which is now NULL).
    // Can be passed in to addPCDataToList() again to append more elements without searching
    // for end-of-list
    return &((*aCTDataPropListPP)->next);
  }
  // nop, return pointer unmodified
  return aCTDataPropListPP;
} // addCTDataPropToList


// add a CTData describing a property (as returned by newDevInfCTData())
// as a new property without parameters to a CTDataPropList
SmlDevInfCTDataPropListPtr_t *addNewPropToList(
  SmlDevInfCTDataPtr_t aPropCTData, // CTData describing property
  SmlDevInfCTDataPropListPtr_t *aCTDataPropListPP // adress of pointer to existing item list or NULL
)
{
  SmlDevInfCTDataPropPtr_t propdataP = SML_NEW(SmlDevInfCTDataProp_t);
  propdataP->param = NULL; // no params
  propdataP->prop = aPropCTData;
  return addCTDataPropToList(propdataP, aCTDataPropListPP);
} // addNewPropToList



// add PCData element to a PCData list
SmlPcdataListPtr_t *addPCDataToList(
  SmlPcdataPtr_t aPCDataP, // Existing PCData element to be added, ownership is passed to list
  SmlPcdataListPtr_t *aPCDataListPP // adress of pointer to existing PCData list or NULL
)
{
  if (aPCDataListPP) {
    // find last PCDataList pointer
    while (*aPCDataListPP) {
      aPCDataListPP=&((*aPCDataListPP)->next);
    }
    // aItemListPP now points to a NULL pointer which must be replaced by addr of new PCDataList entry
    *aPCDataListPP = SML_NEW(SmlPcdataList_t);
    (*aPCDataListPP)->next=NULL;
    (*aPCDataListPP)->data=aPCDataP; // insert new item
    // return pointer to pointer to next element (which is now NULL).
    // Can be passed in to addPCDataToList() again to append more elements without searching
    // for end-of-list
    return &((*aPCDataListPP)->next);
  }
  return NULL;
} // addPCDataToList


// add PCData string to a PCData list
SmlPcdataListPtr_t *addPCDataStringToList(
  const char *aString, // String to be added
  SmlPcdataListPtr_t *aPCDataListPP // adress of pointer to existing PCData list or NULL
)
{
  return addPCDataToList(newPCDataString(aString),aPCDataListPP);
} // addPCDataStringToList


// create new optional location (source or target)
// Returns NULL if URI specified is NULL or empty
SmlSourcePtr_t newOptLocation(
  const char *aLocURI,
  const char *aLocName
)
{
  if (!aLocURI || *aLocURI==0) return NULL;
  else return newLocation(aLocURI,aLocName);
} // newOptLocation


// create new location (source or target)
// always returns location, even if URI and/or name are empty
// If name is NULL or empty, only URI is generated
SmlSourcePtr_t newLocation(
  const char *aLocURI,
  const char *aLocName
)
{
  SmlSourcePtr_t locP;

  locP = SML_NEW(SmlSource_t);
  // URI is always present (might be empty, though)
  locP->locURI=newPCDataString(aLocURI);
  // name only if not empty
  if (aLocName && *aLocName!=0)
    locP->locName=newPCDataString(aLocName);
  else
    locP->locName=NULL;
  // filter defaults to NULL
  locP->filter=NULL;
  return locP;
} // newLocation


// create new empty Item
SmlItemPtr_t newItem(void)
{
  SmlItemPtr_t itemP;

  itemP = SML_NEW(SmlItem_t);
  itemP->target=NULL;
  itemP->source=NULL;
  itemP->meta=NULL;
  itemP->data=NULL;
  // SyncML 1.1, no MoreData set
  itemP->flags=0;
  // SyncML 1.2
  itemP->targetParent=NULL;
  itemP->sourceParent=NULL;
  return itemP;
} // newItem


// create new Item with string-type data
SmlItemPtr_t newStringDataItem(
  const char *aString
)
{
  SmlItemPtr_t itemP=newItem();
  itemP->data=newPCDataString(aString);
  return itemP;
} // newStringDataItem


// create meta-format PCData
SmlPcdataPtr_t newPCDataFormat(
  TFmtTypes aFmtType,
  bool aShowDefault
)
{
  if (aFmtType==fmt_chr && !aShowDefault)
    return NULL; // default
  else
    return newPCDataString(encodingFmtSyncMLNames[aFmtType]); // show format type
} // newPCDataFormat


// create new string-type PCData, if NULL or empty string is passed for aData,
// NULL is returned (optional info not there)
SmlPcdataPtr_t newPCDataFormatted(
  const uInt8 *aData,    // data
  sInt32 aLength,         // length of data, if<=0 then string length is calculated
  TFmtTypes aFmtType,   // encoding Format
  bool aNeedsOpaque     // set opaque needed (string that could confuse XML parsing or even binary)
)
{
  if (!aData) return NULL; // no data
  if (aLength==0) aLength=strlen((const char *)aData);
  if (aLength==0) return NULL; // no data
  // encode input string if needed
  SmlPcdataPtr_t pcdataP;
  char *b64data;
  uInt32 b64len;
  switch (aFmtType) {
    case fmt_b64:
      // convert to b64
      b64len=0;
      b64data=b64::encode(aData, aLength, &b64len);
      pcdataP = newPCDataString(b64data,b64len);
      b64::free(b64data);
      return pcdataP;
    default:
      // just copy into string or opaque/C_DATA string
      return newPCDataStringX(aData, aNeedsOpaque, aLength);
  }
} // newPCDataEncoded


// create new string-type PCData, if NULL or empty string is passed for aString,
// NULL is returned (optional info not there)
SmlPcdataPtr_t newPCDataOptString(
  const char *aString,
  sInt32 aLength // length of string, if<0 then length is calculated
)
{
  if (aString && (*aString!=0))
    return newPCDataString(aString,aLength);
  else
    return NULL;
} // newPCDataOptString


// create new string-type PCData, if NULL is passed for aString,
// NULL is returned (optional info not there)
// if empty string is passed, PCData with empty contents will be created
SmlPcdataPtr_t newPCDataOptEmptyString(
  const char *aString,
  sInt32 aLength // length of string, if<0 then length is calculated
)
{
  if (aString)
    return newPCDataString(aString,aLength);
  else
    return NULL;
} // newPCDataOptEmptyString


// create new string-type PCData, if NULL is passed for aString,
// an empty string is created (that is, a PCData with string terminator as
// content only, length=0)
SmlPcdataPtr_t newPCDataString(
  const char *aString,
  sInt32 aLength // length of string, if<0 then length is calculated
)
{
  return newPCDataStringX((const uInt8 *)aString,false,aLength);
} // newPCDataString


// create new PCData, aOpaque can be used to generate non-string data
// Note: empty strings are always coded as non-opaque, even if aOpaque is set
SmlPcdataPtr_t newPCDataStringX(
  const uInt8 *aString,
  bool aOpaque, // if set, an opaque method (OPAQUE or CDATA) is used
  sInt32 aLength // length of string, if<0 then length is calculated
)
{
  SmlPcdataPtr_t pcdataP;

  pcdataP = SML_NEW(SmlPcdata_t);

  // determine length
  if (aLength>=0 && aString)
    pcdataP->length = aLength; // as specified, and string argument not NULL
  else
    pcdataP->length = aString ? strlen((const char *)aString) : 0; // from argument, if NULL -> length=0
  // determine type
  if (aOpaque && aLength!=0) {
    // Note: due to modification in RTK, this generates
    // OPAQUE in WBXML and CDATA in XML
    pcdataP->contentType=SML_PCDATA_OPAQUE;
  }
  else {
    // non-critical string
    #ifdef SML_STRINGS_AS_OPAQUE
    pcdataP->contentType=SML_PCDATA_OPAQUE;
    #else
    pcdataP->contentType=SML_PCDATA_STRING;
    #endif
  }
  pcdataP->extension=SML_EXT_UNDEFINED;
  // - allocate data space (ALWAYS with room for a terminator, even if Opaque or empty string)
  pcdataP->content=smlLibMalloc(pcdataP->length+1); // +1 for terminator, see below
  // copy data (if any)
  if (pcdataP->length>0) {
    // - copy string
    smlLibMemcpy(pcdataP->content,aString,pcdataP->length);
  }
  // set terminator
  ((char *)(pcdataP->content))[pcdataP->length]=0; // terminate C string
  // return
  return pcdataP;
} // newPCDataStringX


// create new string-type PCData from C++ string
SmlPcdataPtr_t newPCDataString(
  const string &aString
)
{
  return newPCDataString(aString.c_str(),aString.length());
} // newPCDataString(string&)


// create new decimal string representation of sInt32 as PCData
SmlPcdataPtr_t newPCDataLong(
  sInt32 aLong
)
{
  const int ssiz=20;
  char s[ssiz];

  snprintf(s,ssiz,"%ld",(long)aLong);
  return newPCDataString(s);
} // newPCDataLong


// Nonce generator allowing last-session nonce to be correctly re-generated in next session
void generateNonce(string &aNonce, const char *aDevStaticString, sInt32 aSessionStaticID)
{
  md5::SYSYNC_MD5_CTX context;
  uInt8 digest[16];
  md5::Init (&context);
  // - add in static device string
  md5::Update (&context, (const uInt8 *)aDevStaticString, strlen(aDevStaticString));
  // - add in session static ID in binary format
  md5::Update (&context, (const uInt8 *)&aSessionStaticID, sizeof(sInt32));
  // - done
  md5::Final (digest, &context);
  // - make string of first 48 bit of MD5: 48 bits, use 6 bits per char = 8 chars
  uInt64 dig48 = ((uInt32)digest[0] << 0) |
    ((uInt32)digest[1] << 8) |
    ((uInt32)digest[2] << 16) |
    ((uInt32)digest[3] << 24);
  aNonce.erase();
  for (sInt16 k=0; k<8; k++) {
    aNonce+=((dig48 & 0x03F) + 0x21);
    dig48 = dig48 >> 6;
  }
} // generateNonce


// create challenge of requested type
SmlChalPtr_t newChallenge(TAuthTypes aAuthType, const string &aNextNonce, bool aBinaryAllowed)
{
  SmlChalPtr_t chalP=NULL;
  SmlMetInfMetInfPtr_t metaP;

  if (aAuthType!=auth_none) {
    // new challenge record
    chalP = SML_NEW(SmlChal_t);
    // add empty meta
    chalP->meta=newMeta();
    metaP=(SmlMetInfMetInfPtr_t)(chalP->meta->content);
    // add type and format
    // - type
    metaP->type=newPCDataString(authTypeSyncMLNames[aAuthType]);
    // - format
    const char *fmt = NULL;
    switch (aAuthType) {
      case auth_basic:
        // always request b64
        fmt=encodingFmtSyncMLNames[fmt_b64];
        break;
      case auth_md5:
        // request b64 only for non-binary capable encoding (that is, XML)
        /* %%% dont do that, Nokia9210 miserably fails when we do that,
         *     it sends its data B64 encoded, but obviously with bad
         *     data in it. Ericsson T39m seems to do it correctly however.
        if (!aBinaryAllowed)
          fmt=encodingFmtSyncMLNames[fmt_b64];
        */
        // always request b64 for now, seems to be safer with not fully compatible clients
        fmt=encodingFmtSyncMLNames[fmt_b64];
        break;
      default: break;
    }
    metaP->format=newPCDataOptString(fmt); // set format, but not empty
    // - add nonce if needed
    if (aAuthType==auth_md5) {
      // MD5 also might need nonce
      if (!aNextNonce.empty()) {
        // add base64 encoded nonce string
        uInt32 b64len;
        char *b64=b64::encode((const uInt8 *)aNextNonce.c_str(),aNextNonce.size(),&b64len);
        metaP->nextnonce=newPCDataString(b64,b64len);
        b64::free(b64); // return buffer allocated by b64_encode
      }
    }
  }
  return chalP;
} // newChallenge


// create new property or param descriptor for CTCap
SmlDevInfCTDataPtr_t newDevInfCTData(cAppCharP aName,uInt32 aSize, bool aNoTruncate, uInt32 aMaxOccur, cAppCharP aDataType)
{
  SmlDevInfCTDataPtr_t result = SML_NEW(SmlDevInfCTData_t);
  // fill descriptor
  // - name if property or param
  result->name=newPCDataString(aName);
  // - no display name so far
  result->dname=NULL; // no display name
  // - datatype (optional)
  result->datatype=newPCDataOptString(aDataType);
  // - max size
  if (aSize==0)
    result->maxsize=NULL; // no size
  else
    result->maxsize=newPCDataLong(aSize); // set size
  // - no valenum here, will be added later if any
  result->valenum=NULL; // no valenum
  // SyncML 1.2
  if (aMaxOccur==0)
    result->maxoccur=NULL; // no maxoccur
  else
    result->maxoccur=newPCDataLong(aMaxOccur); // set maxoccur
  result->flags = aNoTruncate ? SmlDevInfNoTruncate_f : 0; // notruncate flag or none
  return result;
} // newDevInfCTData


// frees prototype element and sets calling pointer to NULL
void FreeProtoElement(void * &aVoidP)
{
  if (aVoidP) smlFreeProtoElement(aVoidP);
  aVoidP=NULL;
} // FreeProtoElement

} // namespace sysync

// eof
