/*
 * Copyright (c) 2017-2019, Texas Instruments Incorporated
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
 */

/*
 *  ======== rfEasyLinkListenBeforeTalk_nortos.c ========
 */
/* Application header files */
#include "smartrf_settings/smartrf_settings.h"
#include "Board.h"

/* Standard C Libraries */
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* High-level Ti-Drivers */
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/rf/RF.h>
#include <ti/devices/DeviceFamily.h>

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/trng.h)

/* EasyLink API Header files */
#include "easylink/EasyLink.h"

#define RFEASYLINKLBT_PAYLOAD_LENGTH        30

/*
 * Set to 1 if you want to attempt to retransmit a packet that couldn't be
 * transmitted after the CCA
 */
#define RFEASYLINKLBT_RETRANSMIT_PACKETS    1

#if RFEASYLINKLBT_RETRANSMIT_PACKETS
bool bAttemptRetransmission = false;
#endif // RFEASYLINKLBT_RETRANSMIT_PACKETS

/* PIN driver handle */
static PIN_Handle pinHandle;
static PIN_State pinState;

/*
 * Application LED pin configuration table:
 *  - All board LEDs are off
 */
PIN_Config pinTable[] = {
    Board_PIN_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_PIN_LED2 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static uint16_t seqNumber;

static volatile bool lbtDoneFlag;

static uint32_t lastTrngVal;

void HalTRNG_InitTRNG( void )
{
    // configure TRNG
    // Note: Min=4x64, Max=1x256, ClkDiv=1+1 gives the same startup and refill
    //       time, and takes about 11us (~14us with overhead).
    TRNGConfigure( (1 << 8), (1 << 8), 0x01 );

    // enable TRNG
    TRNGEnable();

    // init variable to hold the last value read
    lastTrngVal = 0;
}

void HalTRNG_WaitForReady( void )
{
    // poll status
    while(!(TRNGStatusGet() & TRNG_NUMBER_READY));

    return;
}

uint32_t HalTRNG_GetTRNG( void )
{
    uint32_t trngVal;

    // initialize and enable TRNG if TRNG is not enabled
    if (0 == (HWREG(TRNG_BASE + TRNG_O_CTL) & TRNG_CTL_TRNG_EN))
    {
      HalTRNG_InitTRNG();
    }

    // check that a valid value is ready
    while(!(TRNGStatusGet() & TRNG_NUMBER_READY));

    // check to be sure we're not getting the same value repeatedly
    if ( (trngVal = TRNGNumberGet(TRNG_LOW_WORD)) == lastTrngVal )
    {
      return( 0xDEADBEEF );
    }
    else // value changed!
    {
      // so save last TRNG value
      lastTrngVal = trngVal;

      return( trngVal );
    }
}

void lbtDoneCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        /* Toggle LED1 to indicate TX */
        PIN_setOutputValue(pinHandle, Board_PIN_LED1, !PIN_getOutputValue(Board_PIN_LED1));
#if RFEASYLINKLBT_RETRANSMIT_PACKETS
        bAttemptRetransmission = false;
#endif // RFEASYLINKLBT_RETRANSMIT_PACKETS
    }
    else if (status == EasyLink_Status_Busy_Error)
    {
        /* Toggle LED2 to indicate maximum retries reached */
        PIN_setOutputValue(pinHandle, Board_PIN_LED2, !PIN_getOutputValue(Board_PIN_LED2));

#if RFEASYLINKLBT_RETRANSMIT_PACKETS
        bAttemptRetransmission = true;
#endif // RFEASYLINKLBT_RETRANSMIT_PACKETS
    }
    else
    {
        /* Toggle LED1 and LED2 to indicate error */
        PIN_setOutputValue(pinHandle, Board_PIN_LED1, !PIN_getOutputValue(Board_PIN_LED1));
        PIN_setOutputValue(pinHandle, Board_PIN_LED2, !PIN_getOutputValue(Board_PIN_LED2));
    }

    lbtDoneFlag = true;
}

void *mainThread(void *arg0)
{
    uint32_t absTime;
    EasyLink_TxPacket lbtPacket = { {0}, 0, 0, {0} };

    /* Set power dependency for the TRNG */
    Power_setDependency(PowerCC26XX_PERIPH_TRNG);

    /* Open LED pins */
    pinHandle = PIN_open(&pinState, pinTable);
    if(!pinHandle)
    {
        while(1);
    }

    /* Clear LED pins */
    PIN_setOutputValue(pinHandle, Board_PIN_LED1, 0);
    PIN_setOutputValue(pinHandle, Board_PIN_LED2, 0);

    /* Set the transmission flag to its default state */
    lbtDoneFlag = false;

    // Initialize the EasyLink parameters to their default values
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);

    // Change the RNG function used for clear channel assessment
    easyLink_params.pGrnFxn = (EasyLink_GetRandomNumber)HalTRNG_GetTRNG;

    /* Initialize EasyLink */
    if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        // EasyLink_init failed
        while(1);
    }

    /*
     * If you wish to use a frequency other than the default, use
     * the following API:
     * EasyLink_setFrequency(868000000);
     */

    while(1)
    {
#if RFEASYLINKLBT_RETRANSMIT_PACKETS
        if(bAttemptRetransmission == false)
        {
#endif // RFEASYLINKLBT_RETRANSMIT_PACKETS
            // zero out the packet
            memset(&lbtPacket, 0, sizeof(EasyLink_TxPacket));

            /* Create packet with incrementing sequence number and random payload */
            lbtPacket.payload[0] = (uint8_t)(seqNumber >> 8);
            lbtPacket.payload[1] = (uint8_t)(seqNumber++);

            uint8_t i;
            for(i = 2; i < RFEASYLINKLBT_PAYLOAD_LENGTH; i++)
            {
                lbtPacket.payload[i] = rand();
            }

            lbtPacket.len = RFEASYLINKLBT_PAYLOAD_LENGTH;

            /*
             * Address filtering is enabled by default on the Rx device with the
             * an address of 0xAA. This device must set the dstAddr accordingly.
             */
            lbtPacket.dstAddr[0] = 0xaa;

            /* Set Tx absolute time to current time + 100ms */
            if(EasyLink_getAbsTime(&absTime) != EasyLink_Status_Success)
            {
                // Problem getting absolute time
            }
            lbtPacket.absTime = absTime + EasyLink_ms_To_RadioTime(100);
#if RFEASYLINKLBT_RETRANSMIT_PACKETS
        }
#endif // RFEASYLINKLBT_RETRANSMIT_PACKETS

        // Set the Transmit done flag to false, callback will set it to true
        lbtDoneFlag = false;
        EasyLink_transmitCcaAsync(&lbtPacket, lbtDoneCb);

        /* Wait forever for TX to complete */
        while(lbtDoneFlag == false){};
    }
}


