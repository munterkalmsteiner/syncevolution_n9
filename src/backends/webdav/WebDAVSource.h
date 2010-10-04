/*
 * Copyright (C) 2010 Intel Corporation
 */

#ifndef INCL_WEBDAVSOURCE
#define INCL_WEBDAVSOURCE

#include <config.h>

#ifdef ENABLE_DAV

#include <syncevo/TrackingSyncSource.h>
#include <boost/noncopyable.hpp>
#include "NeonCXX.h"

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * Implements generic access to a WebDAV collection.
 *
 * Change tracking is based on TrackingSyncSource, with the following mapping:
 * - locally unique id = relative URI of resource in collection
 * - revision string = ETag of resource in collection
 */
class WebDAVSource : public TrackingSyncSource, private boost::noncopyable
{
 public:
    WebDAVSource(const SyncSourceParams &params);

 protected:
    /* implementation of SyncSource interface */
    virtual void open();
    virtual bool isEmpty();
    virtual void close();
    virtual Databases getDatabases();

    /* implementation of TrackingSyncSource interface */
    virtual void listAllItems(RevisionMap_t &revisions);
    virtual InsertItemResult insertItem(const string &luid, const std::string &item, bool raw);
    void readItem(const std::string &luid, std::string &item, bool raw);
    virtual void removeItem(const string &uid);

 private:
    
};

SE_END_CXX

#endif // ENABLE_DAV
#endif // INCL_WEBDAVSOURCE