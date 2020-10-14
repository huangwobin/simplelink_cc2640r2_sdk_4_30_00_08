/******************************************************************************

 @file  barometerservice.h

 @brief Barometer service definitions and prototypes

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2012-2020, Texas Instruments Incorporated
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

#ifndef BAROMETERSERVICE_H
#define BAROMETERSERVICE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "st_util.h"

/*********************************************************************
 * CONSTANTS
 */

// Service UUID (base F000****-0451-4000-B000-00000000-0000)
#define BAROMETER_SERV_UUID             0xAA40
#define BAROMETER_DATA_UUID             0xAA41
#define BAROMETER_CONF_UUID             0xAA42
#define BAROMETER_CAL_UUID              0xAA43 // Not used on SensorTag2
#define BAROMETER_PERI_UUID             0xAA44

// Length of sensor data in bytes
#define BAROMETER_DATA_LEN              6

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * API FUNCTIONS
 */


/*
 * Barometer_addService- Initializes the Sensor GATT Profile service
 *          by registering GATT attributes with the GATT server.
 *
 */
extern bStatus_t Barometer_addService(void);

/*
 * Barometer_registerAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
extern bStatus_t Barometer_registerAppCBs(sensorCBs_t *appCallbacks);

/*
 * Barometer_setParameter - Set a Sensor GATT Profile parameter.
 *
 *    param - Profile parameter ID
 *    len - length of data to write
 *    value - pointer to data to read.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16_t will be cast to
 *          uint16_t pointer).
 */
extern bStatus_t Barometer_setParameter(uint8_t param, uint8_t len, void *value);

/*
 * Barometer_getParameter - Get a Sensor GATT Profile parameter.
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16_t will be cast to
 *          uint16_t pointer).
 */
extern bStatus_t Barometer_getParameter(uint8_t param, void *value);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* BAROMETERSERVICE_H */
