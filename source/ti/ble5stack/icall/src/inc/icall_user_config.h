/******************************************************************************
 @file:       icall_user_config.h

 @brief:    to do

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2016-2020, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/
#ifndef ICALL_USER_CONFIG_H
#define ICALL_USER_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ICALL_JT
/*******************************************************************************
 * INCLUDES
 */

#include "rf_hal.h"
#include "hal_assert.h"

/*******************************************************************************
 * TYPEDEFS
 */

PACKED_TYPEDEF_CONST_STRUCT
{
  int8   Pout;
  uint16 txPwrVal;
} txPwrVal_t;

PACKED_TYPEDEF_CONST_STRUCT
{
  txPwrVal_t *txPwrValsPtr;
  uint8       numTxPwrVals;
  int8        defaultTxPwrVal;
} txPwrTbl_t;

typedef const uint32 rfDrvTblPtr_t;

typedef const uint32 eccDrvTblPtr_t;

typedef const uint32 icallServiceTblPtr_t;

typedef const uint32 cryptoDrvTblPtr_t;

typedef const uint32 rtosApiTblPtr_t;

typedef const uint32 trngDrvTblPtr_t;

typedef const uint32 extflashDrvTblPtr_t;

typedef const uint32 tirtosSwiCmdTblPtr_t;

typedef struct
{
  rfDrvTblPtr_t        *rfDrvTbl;
  eccDrvTblPtr_t       *eccDrvTbl;
  cryptoDrvTblPtr_t    *cryptoDrvTbl;
  trngDrvTblPtr_t      *trngDrvTbl;
  rtosApiTblPtr_t      *rtosApiTbl;
  extflashDrvTblPtr_t  *extflashDrvTbl;
} drvTblPtr_t ;

typedef struct
{
  uint8_t         rfFeModeBias;
  regOverride_t  *rfRegTbl;
  regOverride_t  *rfRegTbl1M;
#if defined(BLE_V50_FEATURES) && (BLE_V50_FEATURES & (PHY_2MBPS_CFG | PHY_LR_CFG))
  regOverride_t  *rfRegTbl2M;
  regOverride_t  *rfRegTblCoded;
#endif // PHY_2MBPS_CFG | PHY_LR_CFG
  txPwrTbl_t     *txPwrTbl;
} boardConfig_t;

typedef struct
{
  uint32_t              timerTickPeriod;
  uint32_t              timerMaxMillisecond;
  assertCback_t         *assertCback;
  icallServiceTblPtr_t  *icallServiceTbl;
} applicationService_t ;

typedef struct
{
  const void               *stackConfig;
  const drvTblPtr_t        *drvTblPtr;
  const boardConfig_t      *boardConfig;
  applicationService_t     *appServiceInfo;
} icall_userCfg_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
extern applicationService_t   bleAppServiceInfoTable;

/*********************************************************************
 * FUNCTIONS
 */
extern assertCback_t appAssertCback; // only App's ble_user_config.c
extern assertCback_t halAssertCback; // only Stack's ble_user_config.c

#endif /* ICALL_JT */

#ifdef __cplusplus
}
#endif

#endif /* ICALL_USER_CONFIG_H */
