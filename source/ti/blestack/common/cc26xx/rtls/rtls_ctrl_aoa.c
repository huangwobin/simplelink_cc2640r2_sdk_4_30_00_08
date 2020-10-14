/******************************************************************************

 @file  rtls_ctrl_aoa.c

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

/*********************************************************************
 * INCLUDES
 */

#include <stdint.h>
#include <stdlib.h>

#include "rtls_ctrl_aoa.h"
#include "rtls_ctrl.h"
#include "rtls_host.h"
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <string.h>

/*********************************************************************
 * MACROS
 */


/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

// A single AoA sample
typedef struct
{
  int16_t angle;
  int16_t currentangle;
  int8_t  rssi;
  int16_t signalStrength;
  uint8_t channel;
  uint8_t antenna;
} AoA_Sample;

// Moving average structure
typedef struct
{
    int16_t array[6];
    uint8_t idx;
    uint8_t numEntries;
    uint8_t currentAntennaArray;
    int16_t currentAoA;
    int8_t  currentRssi;
    int16_t currentSignalStrength;
    uint8_t currentCh;
    int32_t AoAsum;
    int16_t AoA;
} AoA_movingAverage;

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Save AoA result
AoA_Results_t gAoaResults;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
AoA_Sample RTLSCtrl_estimateAngle(const AoA_AntennaResult *antA1Result, const AoA_AntennaResult *antA2Result, uint8_t sampleCtrl);

/*********************************************************************
* @fn      RTLSCtrl_postProcessAoa
*
* @brief   Called at the end of each connection event to extract I/Q samples
*
* @param   aoaControlBlock - AoA information saved by RTLS Control
* @param   rssi - rssi to be reported to RTLS Host
*
* @return  none
*/
void RTLSCtrl_postProcessAoa(rtlsAoa_t *aoaControlBlock, int8_t rssi, uint8_t channel)
{
  uint8_t status;
  uint8_t sampleCtrl = aoaControlBlock->aoaParams.sampleCtrl;
  
  if (aoaControlBlock->aoaParams.aoaRole == AOA_ROLE_PASSIVE)
  {
    status = AOA_postProcess(rssi, channel);

    if (status)
    {
      switch(aoaControlBlock->aoaParams.resultMode)
      {
        case AOA_MODE_ANGLE:
        {
          AoA_Sample aoaTempResult;
          rtlsAoaResultAngle_t aoaResult;

          AOA_getPairAngles();

          // Only calculate when we have results from both arrays
          if ((gAoaResults.antA1Result->updated == true  && gAoaResults.antA2Result->updated == true) ||
              (IS_AOA_CONFIG_ONLY_ANT_2(sampleCtrl) && (gAoaResults.antA2Result->updated == TRUE)) ||
              (IS_AOA_CONFIG_ONLY_ANT_1(sampleCtrl) && (gAoaResults.antA1Result->updated == TRUE)) ) 
          {
            gAoaResults.antA1Result->updated = false;
            gAoaResults.antA2Result->updated = false;

            const AoA_AntennaResult *antA1Result = gAoaResults.antA1Result;
            const AoA_AntennaResult *antA2Result = gAoaResults.antA2Result;

            aoaTempResult = RTLSCtrl_estimateAngle(antA1Result, antA2Result, sampleCtrl);

            aoaResult.angle = aoaTempResult.angle;
            aoaResult.antenna = aoaTempResult.antenna;
            aoaResult.rssi = aoaTempResult.rssi;
            aoaResult.channel = aoaTempResult.channel;

            RTLSHost_sendMsg(RTLS_CMD_AOA_RESULT_ANGLE, HOST_ASYNC_RSP, (uint8_t *)&aoaResult, sizeof(rtlsAoaResultAngle_t));
          }
        }
        break;

        case AOA_MODE_PAIR_ANGLES:
        {
          rtlsAoaResultPairAngles_t aoaResult;
          int16_t *pairAngle;
          uint8_t antenna;
          
          AOA_getPairAngles();

          if (gAoaResults.antA1Result->updated == true)
          {
            pairAngle = gAoaResults.antA1Result->pairAngle;
            gAoaResults.antA1Result->updated = false;
            antenna = 1;
          }
          else if (gAoaResults.antA2Result->updated == true)
          {
            pairAngle = gAoaResults.antA2Result->pairAngle;
            gAoaResults.antA2Result->updated = false;
            antenna = 2;
          }
          else
          {
            // Should not get here
            return;
          }

          aoaResult.rssi = rssi;
          aoaResult.channel = channel;
          aoaResult.antenna = antenna;
          
          for (int i = 0; i < AOA_NUM_ANTENNAS; i++)
          {
            aoaResult.pairAngle[i] = pairAngle[i];
          }

          RTLSHost_sendMsg(RTLS_CMD_AOA_RESULT_PAIR_ANGLES, HOST_ASYNC_RSP, (uint8_t *)&aoaResult, sizeof(rtlsAoaResultPairAngles_t));
        }
        break;

        case AOA_MODE_RAW:
        {
          rtlsAoaResultRaw_t *aoaResult;
          uint16_t numAoaSamples;
          uint16_t samplesToOutput;
          AoA_IQSample *pIter;

          aoaResult = RTLSCtrl_malloc(sizeof(rtlsAoaResultRaw_t) + (MAX_SAMPLES_SINGLE_CHUNK * sizeof(AoA_IQSample)));
          pIter = AOA_getRawSamples();

          // If the array is not empty
          if (pIter != NULL && aoaResult != NULL)
          {
            // Calculate how many samples we will be outputting
            numAoaSamples = AOA_calcNumOfCteSamples(aoaControlBlock->aoaParams.cteTime,
                                                    aoaControlBlock->aoaParams.cteScanOvs,
                                                    aoaControlBlock->aoaParams.cteOffset);

            aoaResult->channel = channel;
            aoaResult->rssi = rssi;
            aoaResult->samplesLength = numAoaSamples;
            aoaResult->antenna = AOA_getActiveAnt();
            aoaResult->offset = 0;

            do
            {
              // If the remainder is larger than buff size, tx maximum buff size
              if (aoaResult->samplesLength - aoaResult->offset > MAX_SAMPLES_SINGLE_CHUNK)
              {
                samplesToOutput = MAX_SAMPLES_SINGLE_CHUNK;
              }
              else
              {
                // If not, then output the remaining data
                samplesToOutput = aoaResult->samplesLength - aoaResult->offset;
              }

              // Copy the samples to output buffer
              for (int i = 0; i < samplesToOutput; i++)
              {
                aoaResult->samples[i] = pIter[i];
              }

              RTLSHost_sendMsg(RTLS_CMD_AOA_RESULT_RAW, HOST_ASYNC_RSP, (uint8_t *)aoaResult, sizeof(rtlsAoaResultRaw_t) + (sizeof(AoA_IQSample) * samplesToOutput));

              aoaResult->offset += samplesToOutput;
              pIter += samplesToOutput;
            }
            while (aoaResult->offset < aoaResult->samplesLength);
          }

          if (aoaResult)
          {
            RTLSUTIL_FREE(aoaResult);
          }
        }
        break;

        default:
          break;
      }
    }
    if (IS_AOA_CONFIG_ONLY_ANT_2(sampleCtrl) || IS_AOA_CONFIG_ONLY_ANT_1(sampleCtrl))
    {
      // Only one antenna so stay on current antenna and do not switch to next antenna array
      AOA_setupNextRun(FALSE);
    }
    else
    {
      // Once all post processing is complete, set up the next run
      AOA_setupNextRun(TRUE);
    }
  }
}

/*********************************************************************
* @fn      RTLSCtrl_estimateAngle
*
* @brief   Estimate angle based on I/Q readings
*
* @param   antA1Result - Results from antenna array 1
* @param   antA2Result - Results from antenna array 2
* @param   sampleCtrl - sample control configs: bit 4,5 0x10 - ONLY_ANT_1, 0x20 - ONLY_ANT_2
*
* @return  AoA Sample struct filled with calculated angles
*/
AoA_Sample RTLSCtrl_estimateAngle(const AoA_AntennaResult *antA1Result, const AoA_AntennaResult *antA2Result, uint8_t sampleCtrl)
{
    AoA_Sample AoA;
    uint8_t selectedAntenna = 1;

    static AoA_movingAverage AoA_ma = {0};
    uint8_t AoA_ma_size = sizeof(AoA_ma.array) / sizeof(AoA_ma.array[0]);

    // Calculate AoA for each antenna array
    const int16_t AoA_A1 = ((antA1Result->pairAngle[0] + antA1Result->pairAngle[1]) / 2) + 45 + antA1Result->channelOffset[antA1Result->ch];
    const int16_t AoA_A2 = ((antA2Result->pairAngle[0] + antA2Result->pairAngle[1]) / 2) - 45 - antA2Result->channelOffset[antA2Result->ch];

    // Calculate average signal strength
    const int16_t signalStrength_A1 = (antA1Result->signalStrength[0] + antA1Result->signalStrength[1]) / 2;
    const int16_t signalStrength_A2 = (antA2Result->signalStrength[0] + antA2Result->signalStrength[1]) / 2;

    // Signal strength is higher on A1 vs A2
    if (IS_AOA_CONFIG_ONLY_ANT_1(sampleCtrl))
    {
      selectedAntenna = 1;
    }
    else if (IS_AOA_CONFIG_ONLY_ANT_2(sampleCtrl))
    {
      selectedAntenna = 2;
    }
    else if (antA1Result->rssi > antA2Result->rssi)
    {
      selectedAntenna = 1;
    }
    else
    {
      selectedAntenna = 2;
    }
    
    if (selectedAntenna == 1)
    {
        // Use AoA from Antenna Array A1
        AoA_ma.array[AoA_ma.idx] = AoA_A1;
        AoA_ma.currentAoA = AoA_A1;
        AoA_ma.currentAntennaArray = 1;
        AoA_ma.currentRssi = antA1Result->rssi;
        AoA_ma.currentSignalStrength = signalStrength_A1;
        AoA_ma.currentCh = antA1Result->ch;
        AoA.currentangle = AoA_A1;
    }
    // Signal strength is higher on A2 vs A1
    else
    {
        // Use AoA from Antenna Array A2
        AoA_ma.array[AoA_ma.idx] = AoA_A2;
        AoA_ma.currentAoA = AoA_A2;
        AoA_ma.currentAntennaArray = 2;
        AoA_ma.currentRssi = antA2Result->rssi;
        AoA_ma.currentSignalStrength = signalStrength_A2;
        AoA_ma.currentCh = antA2Result->ch;
        AoA.currentangle = AoA_A2;
    }

    // Add new AoA to moving average
    AoA_ma.array[AoA_ma.idx] = AoA_ma.currentAoA;
    if (AoA_ma.numEntries < AoA_ma_size)
    {
      AoA_ma.numEntries++;
    }
    else
    {
      AoA_ma.numEntries = AoA_ma_size;
    }

    // Calculate new moving average
    AoA_ma.AoAsum = 0;
    for(uint8_t i = 0; i < AoA_ma.numEntries; i++)
    {
        AoA_ma.AoAsum += AoA_ma.array[i];
    }
    AoA_ma.AoA = AoA_ma.AoAsum / AoA_ma.numEntries;

    // Update moving average index
    if(AoA_ma.idx >= (AoA_ma_size - 1))
    {
        AoA_ma.idx = 0;
    }
    else
    {
        AoA_ma.idx++;
    }

    // Return results
    AoA.angle = AoA_ma.AoA;
    AoA.rssi = AoA_ma.currentRssi;
    AoA.signalStrength = AoA_ma.currentSignalStrength;
    AoA.channel = AoA_ma.currentCh;
    AoA.antenna =  AoA_ma.currentAntennaArray;

    return AoA;
}

/*********************************************************************
* @fn      RTLSCtrl_initAoa
*
* @brief   Initialize AoA - has to be called before running AoA
*
* @param   none
*
* @param   startAntenna - start samples with antenna 1 ro 2
*
* @return  none
*/
void RTLSCtrl_initAoa(uint8_t startAntenna)
{
  AOA_init(&gAoaResults, startAntenna);
}

/*********************************************************************
* @fn      RTLSCtrl_aoaEnable
*
* @brief   Enables AoA passing the relevant parameters
*
* @param   aoaControlBlock - AoA information saved by RTLS Control
*
* @return  none
*/
void RTLSCtrl_aoaEnable(rtlsAoa_t *aoaControlBlock)
{
  AOA_cteCapEnable(aoaControlBlock->aoaParams.cteTime,
                   aoaControlBlock->aoaParams.cteScanOvs,
                   aoaControlBlock->aoaParams.cteOffset);
}

/*********************************************************************
* @fn      RTLSCtrl_aoaDisable
*
* @brief   Disables AoA
*
* @param   none
*
* @return  none
*/
void RTLSCtrl_aoaDisable(void)
{
  AOA_cteCapDisable();
}


