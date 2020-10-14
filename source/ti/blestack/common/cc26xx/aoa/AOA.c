/******************************************************************************

 @file  AOA.c

 @brief This file contains methods to enable/disable and control AoA
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

#include <ti/devices/DeviceFamily.h>

#include <stdint.h>
#include <stdlib.h>
#if defined(__IAR_SYSTEMS_ICC__)
#include <intrinsics.h>
#endif

#include <ti/drivers/dpl/ClockP.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>
#include <complex.h>
#include <math.h>
#include "ll_common.h"
#include "AOA.h"
#include "ant_array2_config_boostxl_rev1v1.h"
#include "ant_array1_config_boostxl_rev1v1.h"

#ifdef RTLS_SLAVE
#include "ble_user_config.h"
#endif

// Low level device defines
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_gpt.h)
#include DeviceFamily_constructPath(inc/hw_gpio.h)
#include DeviceFamily_constructPath(inc/hw_ccfg.h)
#include DeviceFamily_constructPath(inc/hw_fcfg1.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include DeviceFamily_constructPath(inc/hw_rfc_rat.h)
#include DeviceFamily_constructPath(inc/hw_rfc_dbell.h)


#include DeviceFamily_constructPath(driverlib/udma.h)

#include <ti/drivers/PIN.h>
#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <ti/drivers/dma/UDMACC26XX.h>
#include <ti/drivers/rf/RF.h>

#include "board.h"

/*******************************************************************************
 * CONSTANTS
 */

#define AOA_NUM_VALID_SAMPLES            8
#define AOA_OFFSET_FIRST_VALID_SAMPLE    8

#define AOA_PACKET_RAM                   0x210000E9

#define AOA_CTE_SCAN_OVS_ADDR            0x210000E3
#define AOA_CTE_OFFSET_ADDR              0x210000E4
#define AOA_CTEINFO_ADDR                 0x210000E9
#define AOA_CTE_TIME_ADDR                0x210000C7

#define AOA_CTE_NO_PROCCESSING_NO_SMPL   0

#define AOA_RES_MAX_SIZE                 511

#define RadToDeg                         180/3.14159265358979323846

#define angleconst                       180/128

#define EVENT_UDMACH14BSEL_EV_RFC_IN_EV4 0x0000005F

#define OVERRIDE_TBL_END                 0xFFFFFFFF

// Antenna switch time - used to optimize AoA
#define AOA_ANT_SWITCH_START_TIME 576

// Signal delay time - used to optimize AoA
#define AOA_SIGNAL_DELAY_TIME 36

/*********************************************************************
 * MACROS
 */

ALLOCATE_CONTROL_TABLE_ENTRY(dmaGpt0ControlTableEntry, UDMA_CHAN_TIMER0_A);
ALLOCATE_CONTROL_TABLE_ENTRY(dmaProg0ControlTableEntry, UDMA_CHAN_DMA_PROG);

#if defined(__TI_COMPILER_VERSION__)
void* my1 = (void *)&dmaGpt0ControlTableEntry;
void* my2 = (void *)&dmaProg0ControlTableEntry;
#elif defined(__IAR_SYSTEMS_ICC__)
__root void* my1 = (void *)&dmaGpt0ControlTableEntry;
__root void* my2 = (void *)&dmaProg0ControlTableEntry;
#endif

/*********************************************************************
 * TYPEDEFS
 */

// AoA report structure
typedef struct
{
  AoA_AntennaConfig *antConfig;
  AoA_AntennaResult *antResult;
  AoA_IQSample *samples;
} AoA_Report_t;

/*********************************************************************
 * LOCAL VARIABLES
 */

// UDMA handle
UDMACC26XX_Handle udmaHandle = NULL;

// PIN handle
PIN_Handle pinHandle = NULL;

// GPTimer handle
GPTimerCC26XX_Handle hTimer;

// GPTimer index
uint32_t timerIdx = Board_GPTIMER0A;

// Pin mask used to toggle antennas
uint32_t pinMask;

// Configurations and results included from antenna array files
AoA_AntennaConfig *antA1Config = &BOOSTXL_AoA_Config_ArrayA1;
AoA_AntennaConfig *antA2Config = &BOOSTXL_AoA_Config_ArrayA2;
AoA_AntennaResult *antA1Result = &BOOSTXL_AoA_Result_ArrayA1;
AoA_AntennaResult *antA2Result = &BOOSTXL_AoA_Result_ArrayA2;

// Global saving the state of the AoA reports
AoA_Report_t gAoaReport = {0};

// Location of CTE override
int32_t gAoaCteOverrideLoc;

// Will be equal to 4*cteScanOvs
uint8_t gAoaNumSamplesPerBlock;

// Number of CTE samples
uint16_t gNumCteSamples;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
int16_t AOA_iatan2sc(int32_t y, int32_t x);
bool AOA_openPins(void);
void AOA_receiverInitHw(void);
void AOA_getRxIQ(uint8_t *packetId, AoA_IQSample **samples);
void AOA_configureHw(AoA_AntennaConfig *config);
int32_t AOA_AngleComplexProductComp(int32_t Xre, int32_t Xim, int32_t Yre, int32_t Yim);

/*********************************************************************
* @fn      iat2
*
* @brief   Evaluate x,y coordinates
*
* @param   none
*
* @return  result - result of evaluation
*/
static inline int16_t iat2(int32_t y, int32_t x)
{
  return ((y*32+(x/2))/x)*2;  // 3.39 mxdiff
}

/*********************************************************************
* @fn      AOA_iatan2sc
*
* @brief   Evaluate x,y coordinates
*
* @param   none
*
* @return  status - success/fail to open pins
*/
int16_t AOA_iatan2sc(int32_t y, int32_t x)
{
  // determine octant
  if (y >= 0) {   // oct 0,1,2,3
    if (x >= 0) { // oct 0,1
      if (x > y) {
        return iat2(-y, -x)/2 + 0*32;
      } else {
        if (y == 0) return 0; // (x=0,y=0)
        return -iat2(-x, -y)/2 + 2*32;
      }
    } else { // oct 2,3
      // if (-x <= y) {
      if (x >= -y) {
        return iat2(x, -y)/2 + 2*32;
      } else {
        return -iat2(-y, x)/2 + 4*32;
      }
    }
  } else { // oct 4,5,6,7
    if (x < 0) { // oct 4,5
      // if (-x > -y) {
      if (x < y) {
        return iat2(y, x)/2 + -4*32;
      } else {
        return -iat2(x, y)/2 + -2*32;
      }
    } else { // oct 6,7
      // if (x <= -y) {
      if (-x >= y) {
        return iat2(-x, y)/2 + -2*32;
      } else {
        return -iat2(y, -x)/2 + -0*32;
      }
    }
  }
}

/*********************************************************************
* @fn      AOA_angleComplexProductComp
*
* @brief   Example code to process I/Q samples
*
* @param   Xre, Xim, Yre, Yim - real and imaginary coordinates
*
* @return  result - angle*angleconst
*/
int32_t AOA_AngleComplexProductComp(int32_t Xre, int32_t Xim, int32_t Yre, int32_t Yim)
{
  int32_t Zre, Zim;
  int16_t angle;

  // X*conj(Y)
  Zre = Xre*Yre + Xim*Yim;
  Zim = Xim*Yre - Xre*Yim;

  // Angle. The angle is returned in 256/2*pi format [-128,127] values
  angle = AOA_iatan2sc((int32_t) Zim, (int32_t) Zre);

  return (angle * angleconst);
}

/*********************************************************************
* @fn      AOA_openPins
*
* @brief   Open pins used for antenna board
*
* @param   none
*
* @return  status - success/fail to open pins
*/
bool AOA_openPins(void)
{
	uint32_t pinCfg;
	PIN_State pinState;

	pinMask = (1 << IOID_27 | 1 << IOID_28 | 1 << IOID_29 | 1 << IOID_30);
	pinCfg = PIN_TERMINATE;
	pinHandle = PIN_open(&pinState, &pinCfg);

	while (pinMask)
	{
#if defined(__TI_COMPILER_VERSION__)
		uint32_t ioid = 31 - __clz(pinMask);
#elif defined(__IAR_SYSTEMS_ICC__)
		uint32_t ioid = 31 - __CLZ(pinMask);
#endif
		pinCfg = ioid | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_INPUT_DIS | PIN_DRVSTR_MED;

		if (PIN_add(pinHandle, pinCfg) != PIN_SUCCESS)
		{
			break;
		}
		pinMask ^= 1 << ioid;
	}

	if (pinMask)
	{
		PIN_close(pinHandle); // If remaining pins, we failed to open all.
		return false;
	}

	pinMask = (1 << IOID_27 | 1 << IOID_28 | 1 << IOID_29 | 1 << IOID_30);
	return true;
}

/*********************************************************************
* @fn      AOA_receiverInitHw
*
* @brief   Initialize Timer + DMA
*
* @param   none
*
* @return  none
*/
void AOA_receiverInitHw(void)
{
	// Chain:
	// 0. GPTn match every 4 us, halted
	// 1. RFC_IN_EV4 -> UDMACH14BSEL,
	// 2. UDMA14B ctl entry -> write 1 to GPTn:CTL:TnEN
	// 3. GPTn:DMAEV:TxMDMAEN = 1
	// 4. UDMACH9xSEL <- GPTnx_DMABREQ
	// 5. UDMA19 ctl entry -> *toggle++ --> GPIOTGL

	///// 0. Set up GPTIMER, compare match every 4us     ///////
	GPTimerCC26XX_Params params;
	GPTimerCC26XX_Params_init(&params);
	params.width          = GPT_CONFIG_16BIT;
	params.mode           = GPT_MODE_PERIODIC_UP;
	params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_OFF;
	hTimer = GPTimerCC26XX_open(timerIdx, &params);

	// Set direction down
	HWREGBITW(GPT0_BASE + GPT_O_TAMR, GPT_TAMR_TACDIR_BITN) = (GPT_TAMR_TACDIR_DOWN >> GPT_TAMR_TACDIR_S);

	ClockP_FreqHz freq;
	ClockP_getCpuFreq(&freq);
	GPTimerCC26XX_Value loadVal = freq.lo / 250000 - 1;
	GPTimerCC26XX_setLoadValue(hTimer, loadVal);

	///// 1. RFC_IN_EV4 route to DMACH14                 ///////
	HWREG(EVENT_BASE + EVENT_O_UDMACH14BSEL) = EVENT_UDMACH14BSEL_EV_RFC_IN_EV4;

	///// 2. Set up DMACH14, 1 to GPTn:CTL:TnEN register ///////
	if (NULL == udmaHandle) udmaHandle = UDMACC26XX_open();

	uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_DMA_PROG | UDMA_PRI_SELECT, //ch #14
												UDMA_SIZE_32 | UDMA_SRC_INC_NONE |
												UDMA_DST_INC_NONE |
												UDMA_ARB_1);


	///// 3. Enable DMA trigger on GPT compare match ///////
	HWREGBITW(GPT0_BASE + GPT_O_DMAEV, GPT_DMAEV_TATODMAEN_BITN) = 1;

	///// 4. Connect GPT0 DMA req to Channel 9 / DMA_CHAN_TIMER0_A ///////
	HWREG(EVENT_BASE + EVENT_O_UDMACH9SSEL) = EVENT_UDMACH9SSEL_EV_GPT0A_DMABREQ;

	///// 5. Copy a toggle entry into GPIOTGL on timer match ///////
	uDMAChannelControlSet(UDMA0_BASE, UDMA_CHAN_TIMER0_A | UDMA_PRI_SELECT, //ch #9
												UDMA_SIZE_32 | UDMA_SRC_INC_32 |
												UDMA_DST_INC_NONE |
												UDMA_ARB_1);
}

/*********************************************************************
* @fn      AOA_init
*
* @brief   Initialize AoA for the defined role
*
* @param   aoaResults - result structure
* @param   startAntenna - start samples with antenna 1 or 2
*
* @return  none
*/
void AOA_init(AoA_Results_t *aoaResults, uint8_t startAntenna)
{
#ifdef RTLS_PASSIVE
  // Initialize antenna toggling patterns
  BOOSTXL_AoA_AntennaPattern_A1_init();
  BOOSTXL_AoA_AntennaPattern_A2_init();

  antA1Result->updated = false;
  antA2Result->updated = false;

  aoaResults->antA1Result = antA1Result;
  aoaResults->antA2Result = antA2Result;

  if (startAntenna == 2)
  {
    // Start with array A2
    gAoaReport.antConfig = antA2Config;
    gAoaReport.antResult = antA2Result;
  }
  else
  {
    // Start with array A1
    gAoaReport.antConfig = antA1Config;
    gAoaReport.antResult = antA1Result;
  }

  // Must init Hw before configuring
  if (udmaHandle == NULL)
  {
    // Initial configuration
    AOA_receiverInitHw();
    AOA_configureHw(antA1Config);
  }

  if (pinHandle == NULL)
  {
    AOA_openPins();
  }
#endif

#ifdef RTLS_SLAVE
  // AoA CTE overrides are the last override (the table finishes with a marker equal to 0xFFFFFFFF)
  gAoaCteOverrideLoc = -1;

  // Find the location of AoA CTE override
  while (rfRegTbl[gAoaCteOverrideLoc + 1].hwRegOverride != OVERRIDE_TBL_END)
  {
    gAoaCteOverrideLoc++;
  }
#endif
}

/*********************************************************************
* @fn      AOA_getRxIQ
*
* @brief   Get packet Id and first sample from RF Core RAM
*
* @param   aoaResults - result structure
*
* @return  none
*/
void AOA_getRxIQ(uint8_t *packetId, AoA_IQSample **samples)
{
  if (packetId != NULL)
  {
    // Should only be called when RF Core is awake
    *packetId = (*((uint8_t*)AOA_PACKET_RAM));
    *samples = packetId ? (AoA_IQSample *)&HWREG(RFC_RAM_BASE + 0x0000C000):NULL;
  }
}

/*********************************************************************
* @fn      AOA_getPairAngles
*
* @brief   Extract results and estimates an angle between two antennas
*
* @param   aoaResults - result structure
*
* @return  none
*/
void AOA_getPairAngles(void)
{
  const uint16_t numReps = (gNumCteSamples / (gAoaReport.antConfig->numAntennas * gAoaNumSamplesPerBlock));
  const uint8_t numAnt = gAoaReport.antConfig->numAntennas;
  const uint8_t numPairs = gAoaReport.antConfig->numPairs;

  // Average relative angle across repetitions
  int32_t antenna_versus_avg[6][6] = {0};
  int32_t antenna_versus_cnt[6][6] = {0};

  for (uint16_t r = 1; r < numReps; ++r)
  {
    for (uint16_t i = AOA_OFFSET_FIRST_VALID_SAMPLE; i < AOA_NUM_VALID_SAMPLES + AOA_OFFSET_FIRST_VALID_SAMPLE; ++i)
    {
      // Loop through antenna pairs and calculate phase difference
      for (uint8_t pair = 0; pair < numPairs; ++pair)
      {
        const AoA_AntennaPair *p = &gAoaReport.antConfig->pairs[pair];
        uint8_t a = p->a; // First antenna in pair
        uint8_t b = p->b; // Second antenna in pair

        // Calculate the phase drift across one antenna repetition (X * complex conjugate (Y))
        int16_t Paa_rel = AOA_AngleComplexProductComp(gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock     + a*gAoaNumSamplesPerBlock + i].i,
                                                      gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock     + a*gAoaNumSamplesPerBlock + i].q,
                                                      gAoaReport.samples[32 + (r-1)*numAnt*gAoaNumSamplesPerBlock + a*gAoaNumSamplesPerBlock + i].i,
                                                      gAoaReport.samples[32 + (r-1)*numAnt*gAoaNumSamplesPerBlock + a*gAoaNumSamplesPerBlock + i].q);

        // Calculate phase difference between antenna a vs. antenna b
        int16_t Pab_rel = AOA_AngleComplexProductComp(gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock + a*gAoaNumSamplesPerBlock + i].i,
                                                      gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock + a*gAoaNumSamplesPerBlock + i].q,
                                                      gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock + b*gAoaNumSamplesPerBlock + i].i,
                                                      gAoaReport.samples[32 + r*numAnt*gAoaNumSamplesPerBlock + b*gAoaNumSamplesPerBlock + i].q);

        // Add to averages
        // v-- Correct for angle drift / ADC sampling frequency error
        antenna_versus_avg[a][b] += Pab_rel + ((Paa_rel * abs(a-b)) / numAnt);
        antenna_versus_cnt[a][b] ++;
      }
    }
  }

  // Calculate the average relative angles
  for (int i = 0; i < numAnt; ++i)
  {
    for (int j = 0; j < numAnt; ++j)
    {
      antenna_versus_avg[i][j] /= antenna_versus_cnt[i][j];
    }
  }

  // Write back result for antenna pairs
  for (uint8_t pair = 0; pair < numPairs; ++pair)
  {
    const AoA_AntennaPair *p = &gAoaReport.antConfig->pairs[pair];
    gAoaReport.antResult->pairAngle[pair] = (int)((p->sign * antenna_versus_avg[p->a][p->b] + p->offset) * p->gain);
  }

  // Mark that this report was processed
  gAoaReport.antResult->updated = true;
}


/*********************************************************************
* @fn      AOA_toggleMaker
*
* @brief   Sets the antenna pattern
*
* @param   in - antenna pattern
* @param   initState - initialized or not
* @param   len - length of in
* @param   out - result buffer
*
* @return  none
*/
void AOA_toggleMaker(const uint32_t *in, uint32_t initState, uint32_t len, uint32_t *out)
{
	uint32_t currState = initState;

	for(int i = 0; i < len; ++i)
	{
			uint32_t tgl = currState ^ in[i];
			currState ^= tgl;
			out[i] = tgl;
	}
}

/*********************************************************************
* @fn      AOA_configureHw
*
* @brief   Configures the HW for a single AoA run
*
* @param   config - structure containing information per antenna array
*
* @return  none
*/
void AOA_configureHw(AoA_AntennaConfig *config)
{
  AoA_Pattern *pattern = config->pattern;
  static volatile uint32_t timerEnableWord = 1;

  HWREG(GPIO_BASE + GPIO_O_DOUTSET31_0) =   config->pattern->initialPattern  & pinMask;
  HWREG(GPIO_BASE + GPIO_O_DOUTCLR31_0) = (~config->pattern->initialPattern) & pinMask;

  uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_TIMER0_A | UDMA_PRI_SELECT, //ch #9
                         UDMA_MODE_BASIC, //single transfer
                         (void *)pattern->toggles, //source address
                         (void *)(GPIO_BASE + GPIO_O_DOUTTGL31_0), //destination address
                         pattern->numPatterns);
  uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_TIMER0_A);


  uDMAChannelTransferSet(UDMA0_BASE, UDMA_CHAN_DMA_PROG | UDMA_PRI_SELECT, //ch #14
                         UDMA_MODE_BASIC, //single transfer
                         (void *)&timerEnableWord,//source address
                         (void *)(GPT0_BASE + GPT_O_CTL),
                         timerEnableWord);

  uDMAChannelEnable(UDMA0_BASE, UDMA_CHAN_DMA_PROG);

  // First period is 4us guard time + 8us settle + 4us first period = 16us
  HWREG(GPT0_BASE + GPT_O_TAV) = AOA_ANT_SWITCH_START_TIME + AOA_SIGNAL_DELAY_TIME;
}

/*********************************************************************
* @fn      AOA_cteCapEnable
*
* @brief   This function enables the CTE capture in the rf core
*
* @param   None
*
* @return  None
*/
void AOA_cteCapEnable(uint8_t cteTime, uint8_t cteScanOvs, uint8_t cteOffset)
{
#if RTLS_PASSIVE
    *((uint8_t*)AOA_CTE_SCAN_OVS_ADDR) = cteScanOvs;
    *((uint8_t*)AOA_CTE_OFFSET_ADDR) = cteOffset;
    *((uint8_t*)AOA_CTE_TIME_ADDR) = cteTime;

    gAoaNumSamplesPerBlock = 4*cteScanOvs;
    gNumCteSamples = AOA_calcNumOfCteSamples(cteTime, cteScanOvs, cteOffset);
#endif

#ifdef RTLS_SLAVE
    rfRegTbl[gAoaCteOverrideLoc].hwRegOverride = (hwOverride_t)((cteTime << 16) | 0x89F3);
#endif
}

/*********************************************************************
* @fn      AOA_cteCapDisable
*
* @brief   This function disables the CTE capture in the rf core
*
* @param   None
*
* @return  None
*/
void AOA_cteCapDisable(void)
{
#ifdef RTLS_PASSIVE
  *((uint8_t*)AOA_CTE_SCAN_OVS_ADDR) = AOA_CTE_NO_PROCCESSING_NO_SMPL;
#endif

#ifdef RTLS_SLAVE
    rfRegTbl[gAoaCteOverrideLoc].hwRegOverride = (hwOverride_t)((AOA_CTE_NO_PROCCESSING_NO_SMPL << 16) | 0x89F3);
#endif
}

/*********************************************************************
* @fn      AOA_calcNumOfCteSamples
*
* @brief   This function calculate the number of IQ samples based
*          on the cte parameters from the CTEInfo header and our
*          patch params
*
* @param   cteTime    - CTEInfo parameter defined in spec
* @param   cteScanOvs - used to enable CTE capturing and set the
*                       sampling rate in the IQ buffer
* @param   cteOffset  - number of microseconds from the beginning
*                       of the tone until the sampling starts
*
* @return  uint16_t - The number of IQ samples to process
*/
uint16_t AOA_calcNumOfCteSamples(uint8_t cteTime, uint8_t cteScanOvs, uint8_t cteOffset)
{
  uint16_t resultSize;
  resultSize = (((cteTime * 8) - cteOffset) * cteScanOvs);

  if (resultSize > AOA_RES_MAX_SIZE)
  {
    return AOA_RES_MAX_SIZE;
  }

  return resultSize;
}

/*********************************************************************
* @fn      AOA_postProcess
*
* @brief   This function will stop the HW timer that toggles the antennas,
* 				 get the Rx I/Q samples from RF Core memory (if they exist) and
* 				 copy them to our memory. Once it's finished it will configure
* 				 the HW for the next run
*
* @param   none
*
* @return  status - indicates caller whether the AoA run was successful
*/
uint8_t AOA_postProcess(int8_t rssi, uint8_t channel)
{
  uint8_t cteRes;
  uint8_t cteTime = *((uint8_t*)AOA_CTE_TIME_ADDR);
  AoA_IQSample *samples;
  uint8_t status = false;
  volatile uint32_t keyHwi;

  GPTimerCC26XX_stop(hTimer);
  AOA_getRxIQ(&cteRes, &samples);

  // Reset the AoA received packet ID
  *((uint8_t*)AOA_PACKET_RAM) = 0x00;

  if (cteRes == cteTime)
  {
    if (samples != NULL)
    {
      gAoaReport.antResult->rssi = rssi;
      gAoaReport.samples = samples;
      gAoaReport.antResult->ch = channel;

      if (gAoaReport.samples != NULL)
      {
        status = true;
      }
      else
      {
        status = false;
      }
    }
  }

  return status;
}

/*********************************************************************
* @fn      AOA_getRawSamples
*
* @brief   Returns pointer to raw I/Q samples
*
* @param   none
*
* @return  Pointer to raw I/Q samples
*/
AoA_IQSample *AOA_getRawSamples(void)
{
  return gAoaReport.samples;
}

/*********************************************************************
* @fn      AOA_getActiveAnt
*
* @brief   Returns active antenna id
*
* @param   none
*
* @return  Pointer to raw I/Q samples
*/
uint8_t AOA_getActiveAnt(void)
{
  if (gAoaReport.antConfig == antA1Config)
  {
    return 1;
  }
  else if (gAoaReport.antConfig == antA2Config)
  {
    return 2;
  }
  return 0;
}

/*********************************************************************
* @fn      AOA_setupNextRun
*
* @brief   Sets up the next AOA run
*
* @param   switchArray - Whether an array switch is required or not
*
* @return  none
*/
void AOA_setupNextRun(uint8_t switchArray)
{
  // Check if a switch of antenna arrays is needed
  if (switchArray)
  {
    // Switch ant array
    if (gAoaReport.antConfig == antA1Config)
    {
      gAoaReport.antConfig = antA2Config;
      gAoaReport.antResult = antA2Result;
    }
    else if (gAoaReport.antConfig == antA2Config)
    {
      gAoaReport.antConfig = antA1Config;
      gAoaReport.antResult = antA1Result;
    }
    else
    {
      // Should not get here
      return;
    }
  }

  // Configure HW for the next run
  AOA_configureHw(gAoaReport.antConfig);
}

