/******************************************************************************

 @file  micro_ble_test_menu.h

 @brief This file contains macros, type definitions, and function prototypes
        for two-button menu implementation.

 Group: WCS BTS
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

#ifndef MICRO_BLE_TEST_MENU_H
#define MICRO_BLE_TEST_MENU_H

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Menus Declarations
 */

/* Main Menu Object */
extern tbmMenuObj_t ubtMenuMain;

/* Items of Main */
extern tbmMenuObj_t ubtMenuInit;
extern tbmMenuObj_t ubtMenuTxPower;
extern tbmMenuObj_t ubtMenuAdvInterval;
extern tbmMenuObj_t ubtMenuAdvChanMap;
extern tbmMenuObj_t ubtMenuTimeToPrepare;
extern tbmMenuObj_t ubtMenuAdvData;
extern tbmMenuObj_t ubtMenuBcastDuty;
extern tbmMenuObj_t ubtMenuBcastControl;

/* Items of (Init) */
extern tbmMenuObj_t ubtMenuAddrStatic;
/* Action items are defined in micro_ble_test.h */

/* Items of (TX Power) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Adv Interval) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Adv Channel Map) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Time to Adv) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Adv Data) */
extern tbmMenuObj_t ubtMenuAdvData;
/* Action items are defined in micro_ble_test.h */

/* Items of (Update Option) */
extern tbmMenuObj_t ubtMenuUpdateOption;
/* Action items are defined in micro_ble_test.h */

/* Items of (Bcast Duty) */
extern tbmMenuObj_t ubtMenuBcastDutyOffTime;
extern tbmMenuObj_t ubtMenuBcastDutyOnTime;

/* Items of (Bcast Duty - Off Time) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Bcast Duty - On Time) */
/* Action items are defined in micro_ble_test.h */

/* Items of (Bcast Control) */
/* Action items are defined in micro_ble_test.h */

#ifdef __cplusplus
}
#endif

#endif /* MICRO_BLE_TEST_MENU_H */

