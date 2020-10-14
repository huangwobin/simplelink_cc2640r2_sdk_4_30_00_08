/**************************************************************************************************
  Filename:       micro_eddystone_beacon.h

  Description:    This file contains the Micro Eddystone Beacon sample application
                  definitions and prototypes.

* Copyright (c) 2015, Texas Instruments Incorporated
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* *  Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* *  Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* *  Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/

#ifndef MICROBLETEST_H
#define MICROBLETEST_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include <menu/two_btn_menu.h>

/*********************************************************************
*  EXTERNAL VARIABLES
*/
/* Main Menu Object */
extern tbmMenuObj_t ubtMenuMain;

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */
/* Actions for Menu: Init - Addr Public */
bool ubTest_doInitAddrPublic(uint8 index);

/* Actions for Menu: Init - Addr Static - Select */
bool ubTest_doInitAddrStaticSelect(uint8 index);

/* Actions for Menu: Init - Addr Static - Generate */
bool ubTest_doInitAddrStaticGenerate(uint8 index);

/* Actions for Menu: TX Power */
bool ubTest_doTxPower(uint8 index);

/* Actions for Menu: Adv Interval */
bool ubTest_doAdvInterval(uint8 index);

/* Actions for Menu: Adv Channel Map */
bool ubTest_doAdvChanMap(uint8 index);

/* Actions for Menu: Time to Adv */
bool ubTest_doTimeToPrepare(uint8 index);

/* Actions for Menu: Adv Data */
bool ubTest_doSetFrameType(uint8 index);

/* Actions for Menu: Update Option */
bool ubTest_doUpdateOption(uint8 index);

/* Actions for Menu: Bcast Duty - On Time */
bool ubTest_doBcastDutyOnTime(uint8 index);

/* Actions for Menu: Bcast Duty - Off Time */
bool ubTest_doBcastDutyOffTime(uint8 index);

/* Actions for Menu: Bcast Control - Start */
bool ubTest_doBcastStart(uint8 index);

/* Actions for Menu: Bcast Control - Stop */
bool ubTest_doBcastStop(uint8 index);

/*
 * Task creation function for the Micro BLE Test.
 */
extern void ubTest_createTask(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* MICROBLETEST_H */
