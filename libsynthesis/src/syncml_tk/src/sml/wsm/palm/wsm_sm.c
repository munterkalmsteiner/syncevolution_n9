/**
 * @file
 * SyncML WorkSpace Manager
 *
 * @target_system   Palm
 * @target_os       PalmOS 3.5
 * @description Storage Management for Workspace Manager API.
 * Palm OS version.
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


/*************************************************************************
 *  Definitions                                                          *
 *************************************************************************/


#include <smldef.h>
#include <smlerr.h>
#include <PalmOS.h>
#include "wsm_sm.h"



/* Global Vars */
/* =========== */

#include "mgr.h"
#define smPalmH   (mgrGetSyncMLAnchor())->wsmGlobals->wsmSm.smPalmH
#define smMemH    (mgrGetSyncMLAnchor())->wsmGlobals->wsmSm.smMemH
#define smLocked  (mgrGetSyncMLAnchor())->wsmGlobals->wsmSm.smLocked


/*************************************************************************
 *  Internal Functions                                                   *
 *************************************************************************/





/*************************************************************************
 *  External Functions                                                   *
 *************************************************************************/


Ret_t smCreate (String_t memName, MemSize_t memSize, MemHandle_t *memH) {

  if ( memSize <= 0 ) {
    return SML_ERR_INVALID_SIZE;
  }

  // only one create call does make sense under Palm OS
  if ( smMemH != 0 ) {
    return SML_ERR_WRONG_USAGE;
  }

  // set new values
  smLocked = 0;
  smMemH = 1;
  *memH = smMemH;

  // create memory
  if ( (smPalmH=MemHandleNew(memSize)) == 0 ) {
    return SML_ERR_NOT_ENOUGH_SPACE;
  }

  return SML_ERR_OK;
}


Ret_t smOpen (String_t memName, MemHandle_t *memH) {

  if ( smMemH == 0 ) {
    return SML_ERR_WRONG_PARAM;
  }

  *memH = smMemH;
  return SML_ERR_OK;
}


Ret_t smClose (MemHandle_t memH) {
  Err ret;

  // reset handle
  if ( (ret=MemHandleFree(smPalmH)) != 0 ) {
    return SML_ERR_WRONG_USAGE;
  }
  smMemH = 0;
  smLocked = 0;

  return SML_ERR_OK;
}


Ret_t smDestroy (String_t memName) {

  return SML_ERR_OK;
}


Ret_t smLock (MemHandle_t memH, MemPtr_t *pMem) {

  if ( memH != smMemH ) {
    return SML_ERR_WRONG_PARAM;
  }
  if ( smLocked ) {
    return SML_ERR_WRONG_USAGE;
  }

  if ( (*pMem = MemHandleLock(smPalmH)) == NULL ) {
    return SML_ERR_UNSPECIFIC;
  }
  smLocked = 1;

  return SML_ERR_OK;
}


Ret_t smUnlock (MemHandle_t memH) {

  if ( memH != smMemH ) {
    return SML_ERR_WRONG_PARAM;
  }
  if ( ! smLocked ) {
    return SML_ERR_WRONG_USAGE;
  }

  if ( MemHandleUnlock(smPalmH) != 0 ) {
    return SML_ERR_UNSPECIFIC;
  }
  smLocked = 0;

  return SML_ERR_OK;
}


Ret_t smSetSize (MemHandle_t memH, MemSize_t newSize) {
  Err ret;

  if ( memH != smMemH ) {
    return SML_ERR_WRONG_PARAM;
  }
  if ( smLocked ) {
    return SML_ERR_WRONG_USAGE;
  }
  if ( newSize <= 0 ) {
    return SML_ERR_INVALID_SIZE;
  }

  if ( (ret=MemHandleResize(smPalmH, newSize)) != 0 ) {
    return SML_ERR_NOT_ENOUGH_SPACE;
  }

  return SML_ERR_OK;
}


Ret_t smGetSize (MemHandle_t memH, MemSize_t *actSize) {

  if ( memH != smMemH ) {
    return SML_ERR_WRONG_PARAM;
  }

  *actSize = MemHandleSize(smPalmH);
  return SML_ERR_OK;
}

