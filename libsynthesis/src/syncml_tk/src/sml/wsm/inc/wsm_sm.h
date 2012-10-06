/**
 * @file
 * SyncML WorkSpace Manager
 *
 * @target_system   All
 * @target_os       All
 * @description Storage Management for Workspace Manager API
 * Encapsulates OS dependent parts of WSM.
 */


/*
 * Copyright Notice
 * Copyright (c) Ericsson, IBM, Lotus, Matsushita Communication
 * Industrial Co., Ltd., Motorola, Nokia, Openwave Systems, Inc.,
 * Palm, Inc., Psion, Starfish Software, Symbian, Ltd. (2001).
 * All Rights Reserved.
 * Implementation of all or part of any Specification may require
 * licenses under third party intellectual property rights,
 * including without limitation, patent rights (such a third party
 * may or may not be a Supporter). The Sponsors of the Specification
 * are not responsible and shall not be held responsible in any
 * manner for identifying or failing to identify any or all such
 * third party intellectual property rights.
 *
 * THIS DOCUMENT AND THE INFORMATION CONTAINED HEREIN ARE PROVIDED
 * ON AN "AS IS" BASIS WITHOUT WARRANTY OF ANY KIND AND ERICSSON, IBM,
 * LOTUS, MATSUSHITA COMMUNICATION INDUSTRIAL CO. LTD, MOTOROLA,
 * NOKIA, PALM INC., PSION, STARFISH SOFTWARE AND ALL OTHER SYNCML
 * SPONSORS DISCLAIM ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
 * HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL ERICSSON, IBM, LOTUS, MATSUSHITA COMMUNICATION INDUSTRIAL CO.,
 * LTD, MOTOROLA, NOKIA, PALM INC., PSION, STARFISH SOFTWARE OR ANY
 * OTHER SYNCML SPONSOR BE LIABLE TO ANY PARTY FOR ANY LOSS OF
 * PROFITS, LOSS OF BUSINESS, LOSS OF USE OF DATA, INTERRUPTION OF
 * BUSINESS, OR FOR DIRECT, INDIRECT, SPECIAL OR EXEMPLARY, INCIDENTAL,
 * PUNITIVE OR CONSEQUENTIAL DAMAGES OF ANY KIND IN CONNECTION WITH
 * THIS DOCUMENT OR THE INFORMATION CONTAINED HEREIN, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH LOSS OR DAMAGE.
 *
 * The above notice and this paragraph must be included on all copies
 * of this document that are made.
 *
 */



#ifndef _WSM_SM_H
#define _WSM_SM_H

#include <smldef.h>


#ifdef __ANSI_C__
/** sbuffer list */
typedef struct smWinList_s {
  char               *memName;      /**< name of buffer */
  char               *winH;         /**< reference to memory block */
  MemHandle_t         memH;         /**< handle of memory block */
  Byte_t              locked;       /**< is handle locked? */
  MemSize_t           memSize;      /**< size of memory block     */
  struct smWinList_s *next;         /**< next list item */
} smWinList_t;
typedef smWinList_t *WsmSmGlobals_t;
#endif

#ifdef __PALM_OS__
#include <PalmOS.h>
/** dynamic buffer array */
typedef struct smPalm_s {
  MemHandle       smPalmH;          /**< reference to only memory block */
  MemHandle_t     smMemH;           /**< handle of only memory block */
  Byte_t          smLocked;         /**< is handle locked? */
} WsmSmGlobals_t;
#endif

#ifdef __EPOC_OS__
/** sbuffer list */
typedef struct smWinList_s {
  char               *memName;      /**< name of buffer */
  char               *winH;         /**< reference to memory block */
  MemHandle_t         memH;         /**< handle of memory block */
  Byte_t              locked;       /**< is handle locked? */
  MemSize_t           memSize;      /**< size of memory block     */
  struct smWinList_s *next;         /**< next list item */
} smWinList_t;
typedef smWinList_t *WsmSmGlobals_t;
#endif


Ret_t smCreate (String_t memName, MemSize_t memSize, MemHandle_t *memH) WSM_FUNC;
Ret_t smOpen (String_t memName, MemHandle_t *memH) WSM_FUNC;
Ret_t smClose (MemHandle_t memH) WSM_FUNC;
Ret_t smDestroy (String_t memName) WSM_FUNC;
Ret_t smLock (MemHandle_t memH, MemPtr_t *pMem) WSM_FUNC;
Ret_t smUnlock (MemHandle_t memH) WSM_FUNC;
Ret_t smSetSize (MemHandle_t memH, MemSize_t newSize) WSM_FUNC;
Ret_t smGetSize (MemHandle_t memH, MemSize_t *actSize) WSM_FUNC;

#endif

