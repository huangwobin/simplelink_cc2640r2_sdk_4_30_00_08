/******************************************************************************

 @file  oad_util.c

 @brief This file contains OAD functions that can be shared between
        persistent application and user applications running either the
        oad_reset_service or the oad service.

        Note: these functions should be used in a TI-RTOS application
        with a BLE-Stack running

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2017-2020, Texas Instruments Incorporated
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
// Needed for Cache operations
#include <driverlib/vims.h>
// Needed for ECC constants
#include <icall/inc/icall_ble_api.h>
#include <common/cc26xx/ecc/ECCROMCC26XX.h>
#include <common/cc26xx/sha2/SHA2CC26XX.h>

#include "oad_util.h"
#include "oad_image_header.h"

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void oadUtilGenerateECDSAWorkzoneBuff(ecdsaSigVerifyBuf_t *);

 /*********************************************************************
 * PUBLIC FUNCTIONS
 */

 /*********************************************************************
 * @fn      OADUtil_signCommand
 *
 * @brief   Verify the signer of an OAD command
 *
 * @param   payload - pointer to command payload
 * @param   len - length of payload
 * @param   inputECCWorkzone - pointer to ECC Workzone RAM
 * @param   inputSHAWorkzone - pointer to SHA Workzone RAM
 *
 * @return  OAD_SUCCESS or OAD_AUTH_FAIL
 */
 uint8_t OADUtil_signCommandECDSA(securityHdr_t * secHdr,
                                    signPld_ECDSA_P256_t *signPld,
                                    uint8_t *payload, uint16_t len)
{
    uint8_t status = OAD_SUCCESS;
    uint8_t oldPairingMode;

    /* First, disable pairing for the duration of the sign/verify procedure
     * This is done because the BLE-stack may use the ECC code during pairing for
     * ECC key generation
     */
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(oldPairingMode),
                            &oldPairingMode);

    uint8_t tempPairingMode = GAPBOND_PAIRING_MODE_NO_PAIRING;
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(tempPairingMode),
                            &tempPairingMode);


    ecdsaSigVerifyBuf_t ecdsaSigVerifyBuf;
    
    //Setup workzone info
    oadUtilGenerateECDSAWorkzoneBuff(&ecdsaSigVerifyBuf);

    // Check if workzone buffers are allocated
    if(ecdsaSigVerifyBuf.eccWorkzone == NULL ||
       ecdsaSigVerifyBuf.SHAWorkzone == NULL ||
       ecdsaSigVerifyBuf.tempWorkzone == NULL )
    {
        // If we are still unable to allocate memory, send a failure code
        return (OAD_NO_RESOURCES);
   }

    // Calculate the data payload length
    if(len <= sizeof(signPld_ECDSA_P256_t))
    {
        return (OAD_AUTH_FAIL);
    }

    uint8_t payloadLen = len - sizeof(signPld_ECDSA_P256_t);

    /*
    * BIM Fxn pointer: Target function defined in bim_main.c
    * Pointer to BIM Fxn: Defined in oad_image_header.h
    */
    bimSignFnPtr_t bimsigPtr = (*(bimSignFnPtr_t *)SIGN_FN_PTR);

    status = bimsigPtr(secHdr->securityVersion,
                       secHdr->timeStamp,
                       payloadLen,
                       payload,
                       (uint8_t *)signPld,
                       &ecdsaSigVerifyBuf);

#ifdef CACHE_AS_RAM
    
    /* If either of the workzones are in the application heap, free them, */
    if(ecdsaSigVerifyBuf.eccWorkzone != NULL )
    {
        ICall_free(ecdsaSigVerifyBuf.eccWorkzone);
    }

    if(ecdsaSigVerifyBuf.SHAWorkzone != NULL )
    {
        ICall_free(ecdsaSigVerifyBuf.SHAWorkzone);
    }
    if(ecdsaSigVerifyBuf.tempWorkzone != NULL)
    {
        ICall_free(ecdsaSigVerifyBuf.tempWorkzone);
    }
    
#else   
    
    // Safely re-enable cache in blocking mode
    VIMSModeSafeSet(VIMS_BASE, VIMS_MODE_ENABLED, true);

    // re-enable paring
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(oldPairingMode),
                          &oldPairingMode);
    
#endif // ifdef CACHE_AS_RAM
    
    return (status);
}

 /*********************************************************************
 * @fn      oadUtilGenerateECDSAWorkzoneBuff
 *
 * @brief   Utility function to determine the location of the ECDSA
 *          workzone.
 *
 * @return  Pointer to workzone or NULL
 */
static void oadUtilGenerateECDSAWorkzoneBuff(ecdsaSigVerifyBuf_t *ecdsaSigVerifyBuf)
{
  ecdsaSigVerifyBuf->eccWorkzone = NULL;
  ecdsaSigVerifyBuf->tempWorkzone = NULL;
  ecdsaSigVerifyBuf->SHAWorkzone = NULL;

  // Allocate space for the ECC workzone
 
#ifdef CACHE_AS_RAM
  
    // Aloocate workzone buffer on heap using ICall_malloc 
  ecdsaSigVerifyBuf->tempWorkzone = ICall_malloc(ECDSA_SHA_TEMPWORKZONE_LEN);
  ecdsaSigVerifyBuf->SHAWorkzone = ICall_malloc(sizeof(SHA256_memory_t));
  // Allocate buffer ECC workzone + 5 buffer for publice key X, Y and Hash buffers and space for their length
  ecdsaSigVerifyBuf->eccWorkzone = ICall_malloc(ECCROMCC26XX_NIST_P256_WORKZONE_SIGN_VERIFY_LEN_IN_BYTES + ECDSA_KEY_LEN*5 + sizeof(uint32_t)*5); // 1280 total bytes
      
#else
    if(ecdsaSigVerifyBuf->eccWorkzone == NULL)
    {   
        // Safely disable the instruction cache in blocking mode
        // this only applies to non Cache as RAM configurations
        VIMSModeSafeSet(VIMS_BASE, VIMS_MODE_DISABLED, true);

        // We haven't been using Cache as RAM, it is safe to assume
        // the entire cache region can be used for workszone
        ecdsaSigVerifyBuf->eccWorkzone = (uint32_t *)GPRAM_BASE;

        // We haven't been using Cache as RAM, it is safe to assume
        // the entire cache region can be used for workszone,
        // The ECC Workzone area is reserved first and SHA comes directly after
        ecdsaSigVerifyBuf->SHAWorkzone = (uint8_t *)(GPRAM_BASE + \
        (ECCROMCC26XX_NIST_P256_WORKZONE_SIGN_VERIFY_LEN_IN_BYTES* \
         sizeof(uint32_t)));

        // The ECC Workzone area is reserved first and SHA comes directly after
        ecdsaSigVerifyBuf->tempWorkzone = (uint8_t *)(GPRAM_BASE + \
        (ECCROMCC26XX_NIST_P256_WORKZONE_SIGN_VERIFY_LEN_IN_BYTES* \
        sizeof(uint32_t)) + sizeof(SHA256_memory_t));

    }
#endif

    return;
}
