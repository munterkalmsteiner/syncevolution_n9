/**
 * @file
 * Compiler Flag Definition File
 *
 * @target_system   palm
 * @target_os       palm
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

/**
 * File for Palm OS Specific Compiler Flags
 */

#ifndef _DEFINE_H
  #define _DEFINE_H

#define __PALM_OS__


#define PILOT_PRECOMPILED_HEADERS_OFF

#define SML_API
#define SML_API_DEF
#define XPT_API
#define XPT_API_DEF

/* Multi segment macro for Palm OS */
#define LIB_FUNC __attribute__ ((section ("lib")))
#define MGR_FUNC __attribute__ ((section ("mgr")))
#define WSM_FUNC __attribute__ ((section ("wsm")))
#define XLT_FUNC __attribute__ ((section ("xlt")))


// SyncML commands which are not required in a client can be switched off

#define ATOMIC_SEND
#define ATOMIC_RECEIVE
#define COPY_SEND
#define COPY_RECEIVE
#define EXEC_SEND
#define EXEC_RECEIVE
#define MAP_RECEIVE
#define MAPITEM_RECEIVE
#define SEARCH_SEND
#define SEARCH_RECEIVE
#define SEQUENCE_SEND
#define SEQUENCE_RECEIVE
#define ADD_SEND
#define GET_SEND
#define RESULT_RECEIVE

/* --- Toolkit a la Carte --- */
/* Compilerflags to include only subsets of the Toolkit functionality */
/* enable Alloc helpers */
#define __USE_ALLOCFUNCS__

/* Compiler Flag to include the XML Parsing Module */
#define __SML_XML__
/* Compiler Flag to include the WBXML Parsing Module */
#define __SML_WBXML__
/* Compiler Flag to include only the Minimum Toolkit functionality */
//#define __SML_LITE__
#define __USE_EXTENSIONS__
#define __USE_METINF__
#define __USE_DEVINF__

#endif
