/*
 *  File:    platform_mutex.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Mutex handling
 *  (Unix implementation)
 *
 *  Copyright (c) 2005-2011 by Synthesis AG + plan44.ch
 *
 *
 */


#include <pthread.h>
#include "platform_mutex.h"


MutexPtr_t newMutex()               {        pthread_mutex_t* m= new pthread_mutex_t;
                                             pthread_mutex_init   ( (pthread_mutex_t*)m, NULL ); return m; }
bool      lockMutex( MutexPtr_t m ) { return pthread_mutex_lock   ( (pthread_mutex_t*)m ) == 0; }
bool   tryLockMutex( MutexPtr_t m ) { return pthread_mutex_trylock( (pthread_mutex_t*)m ) == 0; }
bool    unlockMutex( MutexPtr_t m ) { return pthread_mutex_unlock ( (pthread_mutex_t*)m ) == 0; }
void      freeMutex( MutexPtr_t m ) {        pthread_mutex_destroy( (pthread_mutex_t*)m );
                                                      delete (pthread_mutex_t*)m;   }

/* eof */
