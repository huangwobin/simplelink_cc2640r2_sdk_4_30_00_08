/******************************************************************************

 @file  AOA.h

 @brief This file contains typedefs and API functions of AOA.c
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
 *  @defgroup AOA AOA
 *  @brief This module implements the Angle of Arrival (AOA)
 *
 *  @{
 *  @file  AOA.h
 *  @brief      AOA interface
 */

#ifndef AOA_H_
#define AOA_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include <stdint.h>
#include <driverlib/ioc.h>
   
/*********************************************************************
 * CONSTANTS
 */

#define AOA_NUM_ANTENNAS 3       //!< Number of antennas in antenna array

/*********************************************************************
 * MACROS
 */
/// @brief AOA_PIN Creates bitmap for antenna array pins
#define AOA_PIN(x) (1 << (x&0xff))

/*********************************************************************
 * TYPEDEFS
 */

/** @defgroup AOA_Structs AOA Structures
 * @{
 */

/// @brief AoA Device Role
typedef enum
{
  AOA_ROLE_SLAVE,    //!< Transmitter Role
  AOA_ROLE_MASTER,   //!< Receiver Role
  AOA_ROLE_PASSIVE   //!< Passive Role
} AoA_Role;

/// @brief AoA result per antenna array
typedef struct
{
  uint32_t *signalStrength; //!< Amplitude
  int16_t *pairAngle;       //!< Antenna pair angle
  int8_t *channelOffset;    //!< RF Channel offset
  int8_t rssi;              //!< Last Rx rssi
  uint8_t ch;               //!< Channel
  uint8_t updated;          //!< True if structure has been updated
} AoA_AntennaResult;

/// @brief Combined results from both antenna arrays
typedef struct
{
  AoA_AntennaResult *antA1Result;    //!< results from first antenna array
  AoA_AntennaResult *antA2Result;    //!< results from second antenna array
} AoA_Results_t;

/// @brief AoA Pattern Structure
typedef struct AoA_Pattern
{
  uint8_t   numPatterns;      //!< Number of patterns in toggles
  uint32_t  pinMask;          //!< Pin mask
  uint32_t  initialPattern;   //!< Initial Pattern
  uint32_t  toggles[];        //!< pattern array
} AoA_Pattern;

/// @brief Antenna Pair Structure
typedef struct
{
  uint8_t a ;    //!< First antenna in pair
  uint8_t b ;    //!< Second antenna in pair
  float d;       //!< Variable used in antenna pairs
  int8_t sign;   //!< Sign for the result
  int8_t offset; //!< Measurement offset compensation
  float gain;    //!< Measurement gain compensation
} AoA_AntennaPair;

/// @brief Antenna Configurations structure
typedef struct
{
  uint8_t numAntennas;    //!< Number of antennas
  AoA_Pattern *pattern;   //!< Antenna patterns
  uint8_t numPairs;       //!< Number of antenna pairs
  AoA_AntennaPair *pairs; //!< antenna pair information array
} AoA_AntennaConfig;

/// @brief IQ Sample structure
typedef struct
{
  int16_t q;  //!< Q - Quadrature
  int16_t i;  //!< I- In-phase
} AoA_IQSample;
/** @} End AOA_Structs */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

extern AoA_AntennaConfig BOOSTXL_AoA_Config_ArrayA1;    //!< Configurations included from first antenna array
extern AoA_AntennaResult BOOSTXL_AoA_Result_ArrayA1;    //!< results included from first antenna array

extern AoA_AntennaConfig BOOSTXL_AoA_Config_ArrayA2;    //!< Configurations included from second antenna array
extern AoA_AntennaResult BOOSTXL_AoA_Result_ArrayA2;    //!< results included from second antenna array

/**
* @brief   Extract results and estimates an angle between two antennas
*
* @return  none
*/

void AOA_getPairAngles(void);
/**
* @brief   This function disables the CTE capture in the rf core
*
* @return  None
*/

void AOA_cteCapDisable(void);

/*********************************************************************
* @fn      AOA_setupNextRun
*
* @brief   Sets up the next AOA run
*
* @param   switchArray - Whether an array switch is required or not
*
* @return  none
*/
void AOA_setupNextRun(uint8_t switchArray);

/**
* @brief   Initialize AoA for the defined role
*
* @param   aoaResults - result structure
* @param   startAntenna - start samples with antenna 1 or 2
*
* @return  none
*/
void AOA_init(AoA_Results_t *aoaResults, uint8_t startAntenna);

/**
* @brief   Sets the antenna pattern
*
* @param   in - antenna pattern
* @param   initState - initialized or not
* @param   len - length of in
* @param   out - result buffer
*
* @return  none
*/
void AOA_toggleMaker(const uint32_t *in, uint32_t initState, uint32_t len, uint32_t *out);

/**
* @brief   This function enables the CTE capture in the rf core
*
* @param   cteTime   -  CTETime parameter defined in spec
* @param   cteScanOvs - used to enable CTE capturing and set the
*                       sampling rate in the IQ buffer
* @param   cteOffset -  number of microseconds from the beginning
*                       of the tone until the sampling starts
*
* @return  None
*/
void AOA_cteCapEnable(uint8_t cteTime, uint8_t cteScanOvs, uint8_t cteOffset);

/**
* @brief   This function calculate the number of IQ samples based
*          on the cte parameters from the CTEInfo header and our
*          patch params
*
* @param   cteTime - CTEInfo parameter defined in spec
* @param   cteScanOvs - used to enable CTE capturing and set the
*                       sampling rate in the IQ buffer
* @param   cteOffset - number of microseconds from the beginning
*                       of the tone until the sampling starts
*
* @return  uint16_t - The number of IQ samples to process
*/
uint16_t AOA_calcNumOfCteSamples(uint8_t cteTime, uint8_t cteScanOvs, uint8_t cteOffset);

/**
* @brief   This function will stop the HW timer that toggles the antennas,
* 				 get the Rx I/Q samples from RF Core memory (if they exist) and
* 				 copy them to our memory. Once it's finished it will configure
* 				 the HW for the next run
*
* @param   rssi - Last Rx rssi
* @param   channel - Channel
*
* @return  status - indicates caller whether the AoA run was successful
*/
uint8_t AOA_postProcess(int8_t rssi, uint8_t channel);

/**
* @brief   Returns pointer to raw I/Q samples
*
* @return  Pointer to raw I/Q samples
*/
AoA_IQSample *AOA_getRawSamples(void);

/**
* @brief   Returns active antenna id
*
* @return  Pointer to raw I/Q samples
*/
uint8_t AOA_getActiveAnt(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* AOA_H_ */

/** @} End AOA */
