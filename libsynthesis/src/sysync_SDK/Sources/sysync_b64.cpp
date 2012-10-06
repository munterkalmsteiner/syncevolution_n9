/* b64 encoding/decoding */

#include "prefix_file.h"

#include <stdlib.h>
#include <string.h>

#include "sysync_b64.h"
#include "sync_include.h"

using namespace b64;

static const char table [64] = {

  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',

  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',

  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',

  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};


// free memory allocated with encode or decode above
void b64::free(void *mem)
{
  // we use normal malloc below, so use normal free as well
  ::free(mem);
}



char *b64::encode (const uInt8 *instr, uInt32 len, uInt32 *outlenP, sInt16 maxLineLen, bool crLineEnd) {


  // no line breaks -- this is just a straight b64 transform

  uInt32 i_off = 0;
  uInt32 o_off = 0;
  uInt8 inbuf [3];
  uInt32 inlen,outlen,inover;
  uInt32 triples;
  uInt32 i;
  sInt16 linechars;
  char *outstr = NULL;

  if ( (instr == NULL) || (len == 0) ) {
    return(NULL);
  }

  inlen = len;
  inover = inlen%3;
  triples = ((inlen-inover)/3);

  outlen = 4*triples+1;
  if (inover) {
    outlen+=4;
  }
  if (maxLineLen) {
    // make whole number of quads fit on one line
    maxLineLen &= ~3; // clear bit 0&1
    // also add room for CRs or CRLFs
    outlen +=
      (outlen/maxLineLen+1) << (crLineEnd ? 0 : 1);
  }

  outstr = (char *)malloc(outlen*sizeof(char));
  memset(outstr,0,outlen);

  linechars=0;
  o_off=0;
  for (i = 0; i < triples; i++) {

    i_off = i*3;
    // o_off = i*4; %%% not ok as there might be line ends in between

    inbuf[0] = instr[i_off];
    inbuf[1] = instr[i_off+1];
    inbuf[2] = instr[i_off+2];

    outstr[o_off++] = table[(inbuf [0] & 0xFC) >> 2];
    outstr[o_off++] = table[((inbuf [0] & 0x03) << 4) | ((inbuf [1] & 0xF0) >> 4)];
    outstr[o_off++] = table[((inbuf [1] & 0x0F) << 2) | ((inbuf [2] & 0xC0) >> 6)];
    outstr[o_off++] = table[inbuf [2] & 0x3F];

    // check line wrapping
    linechars+=4;
    if (
      maxLineLen && linechars>=maxLineLen && // line limit enabled
      (i<triples-1 || inover) // and more to come (either full quad or inover padded quad)
    ) {
      if (crLineEnd)
        outstr[o_off++]='\r';
      else {
        outstr[o_off++]=0x0D;
        outstr[o_off++]=0x0A;
      }
      linechars=0;
    }
  }

  if (inover) {
    i_off = i*3;
    // o_off = i*4; %%% not ok as there might be line ends in between

    memset(inbuf,0,3);
    inbuf[0] = instr[i_off];
    if (inover > 1 ) {
      inbuf[1] = instr[i_off+1];
    }
    inbuf[2] = 0;

    outstr[o_off] = table[(inbuf [0] & 0xFC) >> 2];
    outstr[o_off+1] = table[((inbuf [0] & 0x03) << 4) | ((inbuf [1] & 0xF0) >> 4)];
    outstr[o_off+2] = table[((inbuf [1] & 0x0F) << 2) | ((inbuf [2] & 0xC0) >> 6)];
    outstr[o_off+3] = table[inbuf [2] & 0x3F];

    //   generate b64 strings that are padded with = to
    //   make multiple's of 4 (this is how it must be)

    if (inover < 3 ) {
      outstr[o_off+3] = '=';
      if (inover < 2 ) {
        outstr[o_off+2] = '=';
      }
    }
  }

  // output length is either size of all complete quadruples including line feeds (o_off) or 4 more if
  // input was not evenly divisible by 3
  // (%%% luz added case for inover<>0, which produced 4 NULLs at end of string on inover==0)
  if (outlenP) *outlenP = o_off+ (inover==0 ? 0 : 4);

  return(outstr);
}


uInt8 *b64::decode(const char *instr, uInt32 len, uInt32 *outlenP)
{

  uInt8 inbuf [4];
  uInt32 n;
  sInt16 quadi;
  bool done;
  const char *p;
  char c=0;
  uInt8 *outstr,*q;

  // get length if not passed as argument
  if (!instr)
    len=0;
  else
    if (len==0) len=strlen(instr);
  p = instr;

  // this should always be more than enough len:
  // 3 times number of quads touched plus one for NUL terminator
  outstr = (uInt8 *)malloc(((3*(len/4+1))+1) * sizeof(char));
  if (!outstr) return NULL;
  q=outstr;

  // process input string now
  n=0;
  quadi=0; // index within quad
  done=false; // not done yet

  while (!done) {
    if (n<len) {
      // init new quad if needed
      if (quadi==0) {
        // new quad starting, clear buffer
        memset(inbuf,0,4);
      }
      // get next char
      c=*p++;
      n++;
      // process char
      if ((c >= 'A') && (c <= 'Z'))
        c = c - 'A';
      else if ((c >= 'a') && (c <= 'z'))
        c = c - 'a' + 26;
      else if ((c >= '0') && (c <= '9'))
        c = c - '0' + 52;
      else if (c == '+')
        c = 62;
      else if (c == '/')
        c = 63;
      else if (c == '=') {
        // reaching a "=" is like end of data
        done=true;
      }
      else
        continue; // ignore all others
    }
    else
      done=true;
    // save in char
    if (!done) inbuf[quadi++] = c;
    // check if done or full quadruple
    if (done || quadi==4) {
      // produce data now
      if (quadi>=2) {
        // two input bytes, first byte is there for sure
        *q++ = (inbuf [0] << 2) | ((inbuf [1] & 0x30) >> 4);
        if (quadi>=3) {
          // three input bytes, two output bytes are there
          *q++ = ((inbuf [1] & 0x0F) << 4) | ((inbuf [2] & 0x3C) >> 2);
          if (quadi==4) {
            // all 4 bytes there, produce three bytes
            *q++ = ((inbuf [2] & 0x03) << 6) | (inbuf [3] & 0x3F);
          }
        }
      }
      // start new quad
      quadi=0;
    }
  } // while

  // return length if requested
  if (outlenP) *outlenP = q-outstr;
  // make sure output ends with NUL in case it is interpreted as a c string
  *q=0;
  // return string
  return(outstr);


  /*
  unsigned int i_off, o_off;
  uInt8 inbuf [4];
  unsigned int inlen,outlen,inover;
  unsigned int quads;
  int i,cnt;
  uInt8 *outstr = NULL;
  char ch;

  if (len==0) len=strlen(instr);

  if ( (instr == NULL) || (len==0) ) {
    return(NULL);
  }

  inlen = len;
  inover = inlen%4;
  quads = ((inlen-inover)/4);

  // this should always be more than enough len
  outlen = (3*(quads+1))+1;

  outstr = (uInt8 *)malloc(outlen * sizeof(char));
  memset(outstr,0,outlen);
  for (i = 0; i < quads; i++) {

    i_off = i*4;
    o_off = i*3;

    for (cnt = 0; cnt < 4; cnt++) {
      ch = instr[i_off+cnt];

      if ((ch >= 'A') && (ch <= 'Z'))
        ch = ch - 'A';

      else if ((ch >= 'a') && (ch <= 'z'))
        ch = ch - 'a' + 26;

      else if ((ch >= '0') && (ch <= '9'))
        ch = ch - '0' + 52;

      else if (ch == '+')
        ch = 62;

      else if (ch == '=') //no op -- can't ignore this one*
        ch = 0;

      else if (ch == '/')
        ch = 63;

      inbuf[cnt] = ch;

    }

    outstr[o_off] = (inbuf [0] << 2) | ((inbuf [1] & 0x30) >> 4);
    outstr[o_off+1] = ((inbuf [1] & 0x0F) << 4) | ((inbuf [2] & 0x3C) >> 2);
    outstr[o_off+2] = ((inbuf [2] & 0x03) << 6) | (inbuf [3] & 0x3F);
  }

  // handle b64 strings that are not padded correctly

  if (inover) {
    i_off = i*4;
    o_off = i*3;

    memset(inbuf,0,4);

    for (cnt = 0; cnt < inover; cnt++) {
      ch = instr[i_off+cnt];

      if ((ch >= 'A') && (ch <= 'Z'))
        ch = ch - 'A';

      else if ((ch >= 'a') && (ch <= 'z'))
        ch = ch - 'a' + 26;

      else if ((ch >= '0') && (ch <= '9'))
        ch = ch - '0' + 52;

      else if (ch == '+')
        ch = 62;

      else if (ch == '=') //no op -- can't ignore this one*
        ch = 0;

      else if (ch == '/')
        ch = 63;

      inbuf[cnt] = ch;
    }

    outstr[o_off] = (inbuf [0] << 2) | ((inbuf [1] & 0x30) >> 4);
    outstr[o_off+1] = ((inbuf [1] & 0x0F) << 4) | ((inbuf [2] & 0x3C) >> 2);
    outstr[o_off+2] = ((inbuf [2] & 0x03) << 6) | (inbuf [3] & 0x3F);

  }

  if (outlenP) *outlenP = o_off+3;

  return(outstr);
  */
}

// eof
