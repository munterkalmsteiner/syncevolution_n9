/*
 *  File:    sync_declarations.h
 *
 *  Author:  Patrick Ohly <patrick.ohly@intel.com>
 *
 *  C/C++ Programming interface between
 *        the Synthesis SyncML engine
 *        and the database layer
 *
 *  Copyright (c) 2004-2011 by Synthesis AG + plan44.ch
 *
 * These are the declarations for the calling interface between the
 * Synthesis SyncML engine and a UI and/or DB plugin. Use this
 * header file when you only need the declaration (= names) of the
 * various items defined in sync_dbapidef.h. For definitions of
 * enums include engine_defs.h.
 *
 * The following naming convention is used:
 * - struct fooType = struct or class name
 * - typedef struct { } foo_Struct = struct itself (in both C++ and C!)
 * - typedef struct { } *foo = pointer to struct
 *
 * This convention has evolved over time and is not used completely
 * consistently.
 *
 * Also note that some types refer to structs which have no real
 * definition anywhere: they are only introduced to make interfaces
 * type-safe. These pointer types use fooH as naming scheme.
 */

#ifndef SYNC_DECLARATIONS_H
#define SYNC_DECLARATIONS_H

#ifdef __cplusplus
namespace sysync {
#endif

    struct SDK_InterfaceType;
    struct ItemIDType;
    typedef struct ItemIDType ItemID_Struct, *ItemID;
    typedef const struct ItemIDType *cItemID;
    struct MapIDType;
    typedef struct MapIDType MapID_Struct, *MapID;
    typedef const struct MapIDType *cMapID;
    struct TEngineProgressType;
    typedef struct SessionType *SessionH;
    typedef struct KeyType *KeyH;

    /* @TODO: typedef const MapID cMapID: a const pointer or a pointer to const struct?! */

#ifdef __cplusplus
} /* namespace sysync */
#endif


#endif /* SYNC_DECLARATIONS_H */
