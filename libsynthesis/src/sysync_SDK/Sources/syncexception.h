/*
 *  File:         SyncException.cpp
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  TSyncException...
 *    SySync Exception classes
 *
 *  Copyright (c) 2001-2011 by Synthesis AG + plan44.ch
 *
 *  2001-05-28 : luz : created
 *
 */

#ifndef SyncException_H
#define SyncException_H

using namespace std;


namespace sysync {

#if defined(WINCE) || defined(__EPOC_OS__) || defined(ANDROID)
// eVC + EPOC cannot process throw() qualifier
#define NOTHROW
// eVC + EPOC has no exception base class
class exception
{
public:
    exception() {};
    exception(const exception&) {};
    exception& operator= (const exception&) {return *this;};
    virtual ~exception() {};
    virtual const char* what() const {return "exception";};
};
#else
#define NOTHROW throw()
#endif

class TSyncException : public exception
{
  typedef exception inherited;
public:
  TSyncException(const char *aMsg1, localstatus aLocalStatus=LOCERR_EXCEPTION) NOTHROW;
  TSyncException(localstatus aLocalStatus) NOTHROW;
  TSyncException() NOTHROW { fLocalStatus=LOCERR_EXCEPTION; };
  virtual ~TSyncException() NOTHROW;
  virtual const char * what() const NOTHROW;
  localstatus status(void) NOTHROW { return fLocalStatus; }
protected:
  void setMsg(const char *p);
  string fMessage;
private:
  localstatus fLocalStatus;
}; // TSyncException


class TSmlException : public TSyncException
{
  typedef TSyncException inherited;
public:
  TSmlException(const char *aMsg, Ret_t aSmlError) NOTHROW;
  Ret_t getSmlError(void) { return fSmlError; };
private:
  Ret_t fSmlError;
}; // TSmlException



class TStructException : public TSyncException
{
  typedef TSyncException inherited;
public:
  TStructException(const char *aMsg)  NOTHROW: TSyncException (aMsg) {};
}; // TStructException


class TMemException : public TSyncException
{
  typedef TSyncException inherited;
public:
  TMemException(const char *aMsg)  NOTHROW: TSyncException (aMsg) {};
}; // TMemException


} // namespace sysync

#endif  // SyncException_H

// eof
