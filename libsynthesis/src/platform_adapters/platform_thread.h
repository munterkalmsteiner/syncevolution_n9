/*
 *  File:         platform_thread.h
 *
 *  Author:       Lukas Zeller (luz@plan44.ch)
 *
 *  Platform specific thread object implementation
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 *  2004-04-15 : luz : created
 *
 */

#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

#include <generic_types.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#ifdef ANDROID
  #include <pthread.h>
#endif

/*
#ifndef _MSC_VER
  using namespace sysync;
#endif
*/

namespace sysync {


// get id of the running process
uIntArch myProcessID();
// get id of the running thread
uIntArch myThreadID();


class TThreadObject; // forward

// function executed by thread
typedef uInt32 (*TThreadFunc)(TThreadObject *aThreadObject, uIntArch aParam);


// wrapper class for thread
class TThreadObject {
public:
  // creates thread object. Thread ist not started yet, must use launch() for this
  TThreadObject();
  // destroys the thread object
  virtual ~TThreadObject();
  // starts thread (or re-starts it again after termination)
  bool launch(
    TThreadFunc aThreadFunc=NULL, // the function to execute in the thread
    uIntArch aThreadFuncParam=0, // a parameter to pass to the thread
    size_t aStackSize=0, // if 0, default stack size is used
    bool aAutoDispose=false // if true, the thread object will dispose itself when thread has finished running
  );
  // get thread ID
  uIntArch getid(void);
  // soft-terminates thread (sets a flag which requests execute() to terminate
  void terminate(void) { fTerminationRequested=true; };
  // hard (emergency) terminate (aborts processing on the OS level)
  void kill(void);
  // wait for termination of the thread, returns true if so within specified time
  // negative wait time means waiting infinitely.
  bool waitfor(sInt32 aMilliSecondsToWait=0);
  // get exit code of the thread (valid only if thread has already terminated
  uInt32 exitcode(void) { return fExitCode; };
  // This method is the thread function itself
  // - can be derived to create special threads
  //   default behaviour is to call the fThreadFunc with this and fThreadFuncParam
  virtual uInt32 execute(void);
  // checks for termination request
  bool terminationRequested(void) { return fTerminationRequested; };
private:
  // thread options
  uInt32 fStackSize;
  // the thread function
  TThreadFunc fThreadFunc;
  uIntArch fThreadFuncParam;
  // the termination request flag
  bool fTerminationRequested;
  // the exit code
  uInt32 fExitCode;
public:
  // auto disposal of the thread object when thread exits
  bool fAutoDisposeThreadObj;
private:

  #ifdef _WIN32
  // the windows thread
  HANDLE fWinThreadHandle;
  DWORD  fWinThreadId;
  #endif

  #if defined LINUX || defined MACOSX
  // the linux POSIX thread
public:
  pthread_t       fPosixThread;
  pthread_mutex_t fDoneCondMutex;
  pthread_cond_t  fDoneCond;
  bool            fTerminated; // really finished
  #endif
};


} // namespace sysync

#endif // PLATFORM_THREAD_H
/* eof */
