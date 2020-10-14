/******************************************************************************

 @file  rtls_ctrl_aoa.h

 @brief This file contains the functions and structures specific to AoA
 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2018-2020, Texas Instruments Incorporated
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

/**
 *  @defgroup RTLS_CTRL RTLS_CTRL
 *  @brief This module implements Real Time Localization System (RTLS) Control module
 *
 *  @{
 *  @file  rtls_ctrl_aoa.h
 *  @brief      This file contains the functions and structures specific to AoA
 */

#ifndef RTLS_CTRL_AOA_H_
#define RTLS_CTRL_AOA_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

#include "AOA.h"

/*********************************************************************
*  EXTERNAL VARIABLES
*/

/*********************************************************************
 * CONSTANTS
 */

#define MAX_SAMPLES_SINGLE_CHUNK 32    //!< Max number of samples reported in a single chunk when using RAW mode

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/** @defgroup RTLS_CTRL_Structs RTLS Control Structures
 * @{
 */

/// @brief Enumeration for AOA Results modes
typedef enum
{
  AOA_MODE_ANGLE,
  AOA_MODE_PAIR_ANGLES,
  AOA_MODE_RAW
} aoaResultMode_e;

// AoA Parameters - Received from RTLS Node Manager and passed onto RTLS Slave via formed connection
#define AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_MASK       0x30
#define AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_1          0x10
#define AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_2          0x20
#define AOA_CONFIG_SAMPLING_CONTROL_BOTH_ANT_1_AND_2    0x00
#define IS_AOA_CONFIG_ONLY_ANT_1(sampleCtrl)            (((sampleCtrl) & AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_MASK) == AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_1)
#define IS_AOA_CONFIG_ONLY_ANT_2(sampleCtrl)            (((sampleCtrl) & AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_MASK) == AOA_CONFIG_SAMPLING_CONTROL_ONLY_ANT_2)

/// @brief List of AoA parameters
typedef struct __attribute__((packed))
{
  AoA_Role aoaRole;              //!< AoA parameter
  aoaResultMode_e resultMode;    //!< AoA parameter
  uint8_t cteScanOvs;            //!< AoA parameter
  uint8_t cteOffset;             //!< AoA parameter
  uint16_t cteTime;              //!< AoA parameter
  uint8_t sampleCtrl;            //!< AoA parameter
} rtlsAoaParams_t;

/// @brief AoA Enable command
typedef struct __attribute__((packed))
{
  uint8_t enableAoa;    //!< Enable or disable AoA
} rtlsEnableAoaCmd_t;

/// @brief AoA Angle Result
typedef struct __attribute__((packed))
{
  int16_t angle;             //!< AoA Angle Result
  int8_t  rssi;              //!< AoA Angle Result
  uint8_t antenna;           //!< AoA Angle Result
  uint8_t channel;           //!< AoA Angle Result
} rtlsAoaResultAngle_t;

/// @brief AoA Antenna Pairs Result
typedef struct __attribute__((packed))
{
  int8_t  rssi;                          //!< AoA Antenna Pairs Result
  uint8_t antenna;                       //!< AoA Antenna Pairs Result
  uint8_t channel;                       //!< AoA Antenna Pairs Result
  int16_t pairAngle[AOA_NUM_ANTENNAS];   //!< AoA Antenna Pairs Result
} rtlsAoaResultPairAngles_t;

/// @brief AoA Raw Result
typedef struct __attribute__((packed))
{
  int8_t  rssi;                 //!< Rssi for this antenna
  uint8_t antenna;              //!< Antenna array used for this result
  uint8_t channel;              //!< BLE data channel for this measurement
  uint16_t offset;              //!< Offset in RAW result (size of samples[])
  uint16_t samplesLength;       //!< Expected length of entire RAW sample
  AoA_IQSample samples[];       //!< The data itself
} rtlsAoaResultRaw_t;

typedef struct
{
  uint8_t bSlaveAoaParamPend;   //!<  Determines if there are parameters pending to be sent
  rtlsAoaParams_t aoaParams;    //!< AoA Parameters
} rtlsAoa_t;
/** @} End RTLS_CTRL_Structs */

/*********************************************************************
 * API FUNCTIONS
 */

/**
* @brief   Called at the end of each connection event to extract I/Q samples
*
* @param   aoaControlBlock - AoA information saved by RTLS Control
* @param   rssi - rssi to be reported to RTLS Host
* @param   channel - Channel used
*
* @return  none
*/
void RTLSCtrl_postProcessAoa(rtlsAoa_t *aoaControlBlock, int8_t rssi, uint8_t channel);

/**
* @brief   Initialize AoA - has to be called before running AoA
*
* @param   startAntenna - start samples with antenna 1 ro 2
*
* @return  none
*/
void RTLSCtrl_initAoa(uint8_t startAntenna);

/**
* @brief   Enables AoA passing the relevant parameters
*
* @param   aoaControlBlock - AoA information saved by RTLS Control
*
* @return  none
*/
void RTLSCtrl_aoaEnable(rtlsAoa_t *aoaControlBlock);

/**
* @brief   Disables AoA
*
* @return  none
*/
void RTLSCtrl_aoaDisable(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* RTLS_CTRL_AOA_H_ */

/** @} End RTLS_CTRL */
