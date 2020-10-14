/******************************************************************************

 @file  bim_main.c

 @brief This module contains the definitions for the main functionality of a
        Boot  Image Manager.

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

/*******************************************************************************
 *                                          Includes
 */
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <driverlib/flash.h>
#include <driverlib/watchdog.h>
#include <inc/hw_prcm.h>
#include <stddef.h>

#include "hal_flash.h"
#include "ext_flash_layout.h"
#include "crc32.h"
#include "SHA2CC26XX.h"
#include "ext_flash.h"
#include "flash_interface.h"
#include "bim_util.h"
#include "oad_image_header.h"
#include "bim_util.h"
#include "sign_util.h"
#include "oad.h"

#ifdef DEBUG
#include "led_debug.h"
#endif /* DEBUG */

/*******************************************************************************
 * CONSTANTS
 */

#define IMG_HDR_FOUND                  -1
#define EMPTY_METADATA                 -2

#define SUCCESS                         0
#define FAIL                           0xFF
#define SHA_BUF_SZ                      EFL_PAGE_SIZE

/*********************************************************************
 * LOCAL VARIABLES
 */

#if (defined(SECURITY))

#if defined(__IAR_SYSTEMS_ICC__)
__no_init uint8_t shaBuf[SHA_BUF_SZ];
#elif defined(__TI_COMPILER_VERSION__)
uint8_t shaBuf[SHA_BUF_SZ];
#endif

/* Cert element stored in flash where public keys in Little endian format*/
#ifdef __TI_COMPILER_VERSION__
#pragma DATA_SECTION(_secureCertElement, ".cert_element")
#pragma RETAIN(_secureCertElement)
const certElement_t _secureCertElement =
#elif  defined(__IAR_SYSTEMS_ICC__)
#pragma location=".cert_element"
const certElement_t _secureCertElement @ ".cert_element" =
#endif
{
  .version    = SECURE_SIGN_TYPE,
  .len        = SECURE_CERT_LENGTH,
  .options    = SECURE_CERT_OPTIONS,
  .signerInfo = {0xb0,0x17,0x7d,0x51,0x1d,0xec,0x10,0x8b},
  .certPayload.eccKey.pubKeyX = {0xd8,0x51,0xbc,0xa2,0xed,0x3d,0x9e,0x19,0xb7,0x33,0xa5,0x2f,0x33,0xda,0x05,0x40,0x4d,0x13,0x76,0x50,0x3d,0x88,0xdf,0x5c,0xd0,0xe2,0xf2,0x58,0x30,0x53,0xc4,0x2a},
  .certPayload.eccKey.pubKeyY = {0xb9,0x2a,0xbe,0xef,0x66,0x5f,0xec,0xcf,0x56,0x16,0xcc,0x36,0xef,0x2d,0xc9,0x5e,0x46,0x2b,0x7c,0x3b,0x09,0xc1,0x99,0x56,0xd9,0xaf,0x95,0x81,0x63,0x23,0x7b,0xe7}
 };

uint32_t eccWorkzone[SECURE_FW_ECC_NIST_P256_WORKZONE_LEN_IN_BYTES];
uint8_t headerBuf[HDR_LEN_WITH_SECURITY_INFO];

/*******************************************************************************
 * EXTERN FUNCTIONS
 */
extern uint8_t eccRom_verifyHash(uint32_t *, uint32_t *, uint32_t *, uint32_t *,
                                 uint32_t *);
#endif /*if (defined(SECURITY)) */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
static uint32_t Bim_findImageStartAddr(uint32_t efImgAddr, uint32_t imgLen);
#if (defined(SECURITY))
static uint8_t Bim_verifyImage(uint32_t eflStartAddr);
#endif

/*******************************************************************************
 * @fn     Bim_intErasePage
 *
 * @brief  Erase an internal flash page.
 *
 * @param  page - page number
 *
 * @return Zero when successful. Non-zero, otherwise.
 */
static int8_t Bim_intErasePage(uint_least16_t page)
{
    /* Note that normally flash */
    if(FlashSectorErase(page * INTFLASH_PAGE_SIZE) == FAPI_STATUS_SUCCESS)
    {
        return(0);
    }

    return(-1);
}

/*******************************************************************************
 * @fn     Bim_intWriteWord
 *
 * @brief  Write a word to internal flash.
 *
 * @param  addr - address
 * @param  word - value to write
 *
 * @return Zero when successful. Non-zero, otherwise.
 */
static int8_t Bim_intWriteWord(uint_least32_t addr, uint32_t word)
{
    if(FlashProgram((uint8_t *) &word, addr, 4) == FAPI_STATUS_SUCCESS)
    {
        return(0);
    }

    return(-1);
}

/*******************************************************************************
 * @fn     Bim_copyImage
 *
 * @brief  Copies firmware image into the executable flash area.
 *
 * @param  imgStart - starting address of image in external flash.
 * @param  imgLen   - size of image in 4 byte blocks.
 * @param  dstAddr  - destination address within internal flash.
 *
 * @return Zero when successful. Non-zero, otherwise.
 */
static int8_t Bim_copyImage(uint32_t imgStart, uint32_t imgLen,
                                uint32_t dstAddr)
{
    uint_fast16_t page = FLASH_PAGE(dstAddr);
    uint_fast16_t lastPage;

    lastPage = (uint_fast16_t) ((dstAddr + imgLen - 1) / INTFLASH_PAGE_SIZE);

    /* Change image size into word unit */
    imgLen += 3;
    imgLen >>= 2;

    if(dstAddr & 3)
    {
        /* Not an aligned address */
        return(-1);
    }

    if(page > lastPage || lastPage > 30)
    {
        return(-1);
    }

    for (; page <= lastPage; page++)
    {
        uint32_t buf;
        uint_least16_t count = (INTFLASH_PAGE_SIZE >> 2);

        if(page == lastPage)
        {
            /* count could be shorter */
            count = imgLen;
        }

      /* Erase the page */
        if(Bim_intErasePage(page))
        {
            /* Page erase failed */
            return(-1);
        }
        while (count-- > 0)
        {
            /* Read word from external flash */
            if(!extFlashRead(imgStart, 4, (uint8_t *)&buf))
            {
                /* read failed */
                return(-1);
            }
            /* Write word to internal flash */
            if(Bim_intWriteWord(dstAddr, buf))
            {
                /* Program failed */
                return(-1);
            }

            imgStart += 4;
            dstAddr += 4;
            imgLen--;
        }
    }
    /* Do not close external flash driver here just return */
    return(0);
}

/*******************************************************************************
 * @fn     isLastMetaData
 *
 * @brief  Copies the internal application + stack image to the external flash
 *         to serve as the permanent resident image in external flash.
 *
 * @param  flashPageNum - Flash page number to start search for external flash
 *         metadata
 *
 * @return flashPageNum - Valid flash page number if metadata is found.
 *         IMG_HDR_FOUND - if metadat starting form specified flash page not found
 *         EMPTY_METADATA - if it is empty flashif metadata not found.
 */
static int8_t isLastMetaData(uint8_t flashPageNum)
{
    /* Read flash to find OAD image identification value */
    imgFixedHdr_t imgHdr;
    uint8_t readNext = true;
    do
    {
        extFlashRead(EXT_FLASH_ADDRESS(flashPageNum, 0),  OAD_IMG_ID_LEN, (uint8_t *)&imgHdr.imgID[0]);

        /* Check imageID bytes */
        if(metadataIDCheck(&imgHdr) == true)  /* External flash metadata found */
        {
            return(flashPageNum);
        }
        else if(imgIDCheck(&imgHdr) == true)   /* Image metadata found which indicate end of external flash metadata pages*/
        {
           return(IMG_HDR_FOUND);
        }
        flashPageNum +=1;
        if(flashPageNum >= MAX_OFFCHIP_METADATA_PAGES) /* End of flash reached, Note:practically it would never go till end if there a factory image exits */
        {
            readNext = false;
        }

    }while(readNext);

    return(EMPTY_METADATA);
}

#if defined(SECURITY)
/*******************************************************************************
 * @fn         checkForSec
*
*  @brief      Check for Security Payload. Reads through the headers in the .bin
*              file. If a security header is found the function checks to see if
*              the header has a populated payload.
*
*  @param       eFlStrAddr - The start address in external flash of the binary image
*
*  @return      0  - security not found
*  @return      1  - security found
*
*/
bool checkForSec(uint32_t eFlStrAddr, uint32_t imgLen)
{
    bool securityFound = false;
    uint8_t endOfSegment = 0;
    uint8_t segmentType = DEFAULT_STATE;
    uint32_t segmentLength = 0;
    uint32_t searchAddr =  eFlStrAddr+OAD_IMG_HDR_LEN;

    while(!endOfSegment)
    {
        extFlashRead(searchAddr, 1, &segmentType);

        if(segmentType == IMG_SECURITY_SEG_ID)
        {
            /* In this version of BIM, the security header will ALWAYS be present
               But the paylond will sometimes not be there. If this finds the
               header, the payload is also checked for existance. */
            searchAddr += SIG_OFFSET;
            uint32_t sigVal = 0;
            extFlashRead(searchAddr, sizeof(uint32_t), (uint8_t *)&sigVal);
            if(sigVal != 0) //Indicates the presence of a signature
            {
                endOfSegment = 1;
                securityFound = true;
            }
            else
            {
                break;
            }
        }
        else
        {
            searchAddr += SEG_LEN_OFFSET;
            if((searchAddr + sizeof(uint32_t)) > (eFlStrAddr + imgLen))
            {
                break;
            }
            extFlashRead(searchAddr, sizeof(uint32_t), (uint8_t *)&segmentLength);
            searchAddr += (segmentLength - SEG_LEN_OFFSET);
            if((searchAddr) > (eFlStrAddr + imgLen))
            {
                break;
            }
        }
    }

    return securityFound;
}
#endif /* if defined(SECURITY) */
/*******************************************************************************
 * @fn     checkImagesExtFlash
 *
 * @brief  Checks for stored images on external flash. If valid image is found
 * to be copied, it copies the image and if the image is executable it will jump
 * to execute.
 *
 * @param  None
 *
 * @return 1 - If flash open fails or no image found on external flash
 *             number if metadata is found.
 *         EMPTY_METADATA - if metadata not found.
 */
static int8_t checkImagesExtFlash(void)
{
    ExtImageInfo_t metadataHdr;

    int8_t flashPageNum = 0;

    /* Initialize external flash driver. */
    if(!extFlashOpen())
    {
#ifdef DEBUG
        while(1);
#else
        return(FAIL);
#endif
    }

    /* Read flash to find OAD external flash metadata identification value  and check for external
      flash bytes */
    while((flashPageNum = isLastMetaData(flashPageNum)) > -1)
    {
        /* Read whole metadata header */
        extFlashRead(EXT_FLASH_ADDRESS(flashPageNum, 0), EFL_METADATA_LEN, (uint8_t *)&metadataHdr);

        /* check BIM and Metadata version */
        if((metadataHdr.fixedHdr.imgCpStat != NEED_COPY) ||
           (metadataHdr.fixedHdr.bimVer != BIM_VER  || metadataHdr.fixedHdr.metaVer != META_VER) ||
           (metadataHdr.fixedHdr.crcStat == CRC_INVALID))  /* Invalid CRC */
        {
            flashPageNum += 1; /* increment flash page number */
            continue;          /* Continue search on next flash page */
        }

        /* check image CRC status */
        if(metadataHdr.fixedHdr.crcStat == DEFAULT_STATE) /* CRC not calculated */
        {
            /*
             * Calculate the CRC over the data buffer and update status
             * @T.B.D. Skipping this section, BIM shouldn't calculate the CRC,
             * the application should calculate and update it.
             */
        } /* if(metadataHdr.fixedHdr.crcStat == 0xFF) */

        int8_t externalVerifyStatus = FAIL;

#if (defined(SECURITY))
        uint32_t eFlStrAddr = metadataHdr.extFlAddr;  //((uint32_t *)&metadataHdr)[EFL_IMG_STR_ADDR_OFFSET];
        uint8_t securityPresence = checkForSec(eFlStrAddr, metadataHdr.fixedHdr.len);
        if(securityPresence)
        {
            /* Calculate the SHA256 of the image */
            uint8_t readSecurityByte[SEC_VERIF_STAT_OFFSET + 1];

            /* Read in the header to check if the signature has already been denied */
            extFlashRead(eFlStrAddr,(SEC_VERIF_STAT_OFFSET + 1), readSecurityByte);

            if(readSecurityByte[SEC_VERIF_STAT_OFFSET] != VERIFY_FAIL)
            {
                externalVerifyStatus = Bim_verifyImage(eFlStrAddr);

                /* If the signature is invalid, mark the image as invalid */
                if((uint8_t)externalVerifyStatus != SUCCESS)
                {
                    readSecurityByte[SEC_VERIF_STAT_OFFSET] = VERIFY_FAIL;
                    extFlashWrite((eFlStrAddr+SEC_VERIF_STAT_OFFSET), 1,
                                  &readSecurityByte[SEC_VERIF_STAT_OFFSET]);

                }
            }
        } /* if(securityPresence) */
#else
    externalVerifyStatus = SUCCESS;
#endif /* ifdef SECURITY */

        /*
         * We get here, we must have valid CRC
         * Check image copy status,  On CC26XXR2 platform only stack application
         * needes to be copied. Additionally, check that a valid signature was found
         */
        if(metadataHdr.fixedHdr.imgCpStat == NEED_COPY && ((uint8_t)externalVerifyStatus == SUCCESS))
        {
            /* On CC26XXR2 platform only stack application needes to be copied
               Do the image copy */
            uint32_t eFlStrAddr = metadataHdr.extFlAddr;
            uint8_t status = COPY_DONE;

           /* Now read image's start address from image stored on external flash */
           imgFixedHdr_t imgHdr;

           /* Read whole image header, to find the image start address */
           extFlashRead(eFlStrAddr, sizeof(imgFixedHdr_t),/*OAD_IMG_FULL_HDR_LEN*/ (uint8_t *)&imgHdr);

           /* Copy image on internal flash */

           /* Get image start address from image */
           uint32_t startAddr = INVALID_ADDR;
           startAddr = Bim_findImageStartAddr(eFlStrAddr, (uint32_t)imgHdr.len);
           if(startAddr == INVALID_ADDR)
           {
               continue; // Try to find another image
           }
           /*
            * NOTE:@TBD during debugging image length wouldn't available, as it is updated
            * by python script during post build process, for now this implementation
            * catering only for contiguous image's, so image length will be calculate
            * by subtracting the image end adress by image start address.
            */
#ifdef DEBUG
            imgHdr.len = imgHdr.imgEndAddr - startAddr + 1;
#endif

            uint8_t retVal = Bim_copyImage(eFlStrAddr, imgHdr.len, startAddr);
            extFlashWrite(EXT_FLASH_ADDRESS(flashPageNum, IMG_COPY_STAT_OFFSET), 1, (uint8_t *)&status);

           /* If image copy is successful */
           if(retVal == SUCCESS)
           {

                /* update image copy status and calculate the
                the CRC of the copied
                and update it's CRC status. CRC_STAT_OFFSET
                */
                uint32_t crc32 = crcCalc(FLASH_PAGE(startAddr), 0, false);
                status = CRC_VALID;
                if(crc32 == imgHdr.crc32) // if crc matched then update its status in the copied image
                {
                    extFlashWrite(FLASH_ADDRESS(flashPageNum, CRC_STAT_OFFSET), sizeof(status), &status);
                    uint8_t securityStatus = VERIFY_FAIL;
#ifdef SECURITY
                    if(securityPresence)
                    {
                        uint8_t readSecurityByte[SEC_VERIF_STAT_OFFSET + 1];

                        /* Read in the onchip header to get the image signature validity */
                        memCpy(readSecurityByte,(void const*)startAddr, (SEC_VERIF_STAT_OFFSET + 1));

                        if(readSecurityByte[SEC_VERIF_STAT_OFFSET] != VERIFY_FAIL)
                        {
                            /* Calculate the SHA256 of the image */
                            /* Hash the onchip image */
                            uint8_t *dataHash = NULL;
                            dataHash = computeSha2Hash(startAddr, shaBuf, SHA_BUF_SZ, false);
                            
                            if(dataHash == NULL)
                            {
                              continue; // Search for another image
                            }

                            /* Read in the onchip header to get the image signature */
                            memCpy(headerBuf, (void *)startAddr, HDR_LEN_WITH_SECURITY_INFO);

                            /* Verify the image signature using public key, image hash, and signature */
                            // Create temp buffer used for ECDSA sign verify, it should 6*ECDSA_KEY_LEN
                            uint8_t tempWorkzone[ECDSA_SHA_TEMPWORKZONE_LEN];
                            uint8_t internalVerifyStatus = bimVerifyImage_ecc(
                                                   _secureCertElement.certPayload.eccKey.pubKeyX,
                                                   _secureCertElement.certPayload.eccKey.pubKeyY,
                                                    dataHash,
                                                    &headerBuf[SEG_SIGNR_OFFSET],
                                                    &headerBuf[SEG_SIGNS_OFFSET],
                                                    eccWorkzone,
                                                    tempWorkzone);

                            /* If the signature is invalid, mark the image as invalid */
                            if((uint8_t)internalVerifyStatus != SECURE_FW_ECC_STATUS_VALID_SIGNATURE)
                            {
                                securityStatus = VERIFY_FAIL;
                                extFlashWrite((eFlStrAddr+SEC_VERIF_STAT_OFFSET), 1, &securityStatus);
                                writeFlash((startAddr+SEC_VERIF_STAT_OFFSET), &securityStatus, 1);
                            }
                            else
                            {
                                securityStatus = VERIFY_PASS;
                            }

                        } /* if(readSecurityByte[SEC_VERIF_STAT_OFFSET] != VERIFY_FAIL) */

                    } /* if(securityPresence) */

                    // If it is security enabled BIM and security information is missing fail validation
#else
     securityStatus = VERIFY_PASS;
#endif /* ifdef SECURITY */

                    // Update internal flash staus 
                    if(imgHdr.imgType == OAD_IMG_TYPE_APP_STACK)
                    {
                         writeFlash((startAddr + (uint32_t)CRC_STAT_OFFSET), (uint8_t *) &status, 1);
                    }

                    /* If it is executable and secure Image jump to execute it */
                    if(((imgHdr.imgType == OAD_IMG_TYPE_APP) ||
                      (imgHdr.imgType == OAD_IMG_TYPE_PERSISTENT_APP) ||
                      (imgHdr.imgType == OAD_IMG_TYPE_APP_STACK)) &&
                      (securityStatus == VERIFY_PASS))
                      {
                          extFlashClose();
                          jumpToPrgEntry(imgHdr.prgEntry);
                      }

                } /* if(crc32 == imgHdr.crc32) */
                else
                {
                    status = CRC_INVALID;
                    extFlashWrite((startAddr + CRC_STAT_OFFSET), 1, &status);
                }
                //update copy status
                status = COPY_DONE;
                extFlashWrite((startAddr + IMG_COPY_STAT_OFFSET), 1, (uint8_t *)&status);
            } /* if(retVal == SUCCESS) */
        }

        /* Unable to find valid executable image, try to find valid user application */
        flashPageNum += 1;

    }  /* end of  while((retVal = isLastMetaData(flashPageNum)) > -1) */

    extFlashClose();
    return(SUCCESS);
}

/*******************************************************************************
 * @fn     checkPgBoundary
 *
 * @brief  Checks if the last page of the external flash page is reached if not
 *         increments the page and returns the page number.
 *
 * @param  flashPageNum - page number of the ext flash.
 *
 * @return None.
 */
uint8_t checkPgBoundary(uint8 *flashPageNum)
{
    *flashPageNum += 1;
    return(*flashPageNum >= MAX_ONCHIP_FLASH_PAGES)? TRUE: FALSE;
}
/*******************************************************************************
 * @fn     Bim_findImageStartAddr
 *
 * @brief  Checks for stored images on the on-chip flash. If valid image is
 * found to be copied, it executable it.
 *
 * @param  efImgAddr - image start address on external flash
 * @param  imgLen    - length of the image
 *
 * @return - start address of the image if image payload segment is found else
 *          INVALID_ADDR
 */
static uint32_t Bim_findImageStartAddr(uint32_t efImgAddr, uint32_t imgLen)
{
    uint8_t buff[12];
    uint32_t startAddr = INVALID_ADDR;
    uint32_t offset = OAD_IMG_HDR_LEN;
    uint32_t len = SEG_HDR_LEN;
    uint32_t flashAddrBase = efImgAddr;
    do
    {
        /* Read flash to find segment header of the first segment */
        if(extFlashRead((flashAddrBase + offset), len, &buff[0]))
        {
            /* Check imageID bytes */
            if(buff[0] == IMG_PAYLOAD_SEG_ID)
            {
                startAddr = *((uint32_t *)(&buff[8]));
                break;
            }
            else /* some segment other than image payload */
            {
                /* get segment payload length and keep iterating */
                uint32_t payloadlen = *((uint32_t *)&buff[4]);
                if(payloadlen == 0) /* We have problem here break to avoid spinning in loop */
                {
                    break;
                }
                else
                {
                    offset += payloadlen;
                }
            }
        }
        else
        {
            break;
        }
    }while(offset < imgLen);

    return startAddr;
}

/*******************************************************************************
 * @fn     checkImagesIntFlash
 *
 * @brief  Checks for stored images on the on-chip flash. If valid image is
 * found to be copied, it executable it.
 *
 * @param  flashPageNum - Flash page number to start searching for imageType.
 * @param  imgType - Image type to look for.
 *
 * @return - No return if image is found else
 *           0 - If indended image is not found.
 */
static uint8_t checkImagesIntFlash(uint8_t flashPageNum)
{
    imgFixedHdr_t imgHdr;

    do
    {
        /* Read flash to find OAD image identification value */
        readFlash((uint32_t)FLASH_ADDRESS(flashPageNum, 0), &imgHdr.imgID[0], OAD_IMG_ID_LEN);

        /* Check imageID bytes */
        if(imgIDCheck(&imgHdr) == TRUE)
        {
            /* Read whole image header */
            readFlash((uint32_t)FLASH_ADDRESS(flashPageNum, 0), (uint8 *)&imgHdr, OAD_IMG_HDR_LEN);

            /* If application is neither executable user application or merged app_stack */
            if(!(OAD_IMG_TYPE_APP == imgHdr.imgType ||
                OAD_IMG_TYPE_APP_STACK == imgHdr.imgType) )
            {
                continue;
            }
#ifdef DEBUG
            if((imgHdr.bimVer == BIM_VER  || imgHdr.metaVer == META_VER))
            {
                jumpToPrgEntry(imgHdr.prgEntry);  /* No return from here */
            }
#endif
            /* Invalid metadata version */
            if((imgHdr.bimVer != BIM_VER  || imgHdr.metaVer != META_VER) ||
               /* Invalid CRC */
               (imgHdr.crcStat == CRC_INVALID))
            {
                continue;
            }
            else if(imgHdr.crcStat == DEFAULT_STATE) /* CRC not calculated */
            {
                uint8_t  crcstat = CRC_VALID;

                /* Calculate the CRC over the data buffer and update status */
                uint32_t crc32 = crcCalc(flashPageNum, 0, false);

                /* Check if calculated CRC matched with the image header */
                if(crc32 != imgHdr.crc32)
                {
                    /* Update CRC status */
                    crcstat = CRC_INVALID;
                }

                /* If JTAG debug, skip the crc checking and updating the crc
                   as crc wouldn't have been calculated status */
                writeFlash((uint32_t)FLASH_ADDRESS(flashPageNum, CRC_STAT_OFFSET), (uint8_t *)&crcstat, 1);
                if(crc32 == imgHdr.crc32)
                {
                    jumpToPrgEntry(imgHdr.prgEntry);  /* No return from here */
                }


                /* If it reached here nothing can be done other than try to find
                   good another image whicn wouldn't be found since it extrenal
                   flash OAD */
            }

            else if(imgHdr.crcStat == CRC_VALID)
            {
                jumpToPrgEntry(imgHdr.prgEntry);  /* No return from here */
            }
        } /* if (imgIDCheck(&imgHdr) == true) */

    } while(flashPageNum++ < (MAX_ONCHIP_FLASH_PAGES -1));  /* last flash page contains CCFG */
    return(0);
}

/*******************************************************************************
 * @fn     getExtFlashSize
 *
 * @brief  Checks for stored images on the on-chip flash. If valid image is
 * found to be copied, it executable it.
 *
 * @param  None.
 *
 * @return None.
 */
void getExtFlashSize(void)
{
    const ExtFlashInfo_t  *pExtFlashInfo = extFlashInfo();
}

/*******************************************************************************
 * @fn     Bim_checkImages
 *
 * @brief  Check for stored images on external flash needed to be copied and
 *         execute. If there is no image to be copied, execute on-chip image.
 *
 * @param  none
 *
 * @return none
 */
void Bim_checkImages(void)
{
#ifndef DEBUG
   
    checkImagesExtFlash();

    /*  Find executable on onchip flash and execute */
    checkImagesIntFlash(0);

#else /* ifdef DEBUG */
    
    int8_t retVal = 0;

    if( !(retVal = checkImagesExtFlash() ) )/* if it traversed through the metadata */
    {
       checkImagesIntFlash(0); /* Find application image on internal flash page and jump */
    }
    if( retVal == -1)
    {
        powerUpGpio();
        lightRedLed();
        while(1);
    }
#endif /* DEBUG */
    
    /* if it reached there is a problem */
    /* TBD put device to sleep and wait for button press interrupt */
    halSleepExec();
}

#if defined(SECURITY)
/*******************************************************************************
 * @fn      Bim_verifyImage
 *
 * @brief   Verifies the image stored on external flash using ECDSA-SHA256
 *
 * @param   eflStartAddr - external flash address of the image to be verified.
 *
 * @return  Zero when successful. Non-zero, otherwise..
 */
static uint8_t Bim_verifyImage(uint32_t eflStartAddr)
{
    uint8_t verifyStatus = FAIL;

    /* Read in the offchip header to get the image signature */
    extFlashRead(eflStartAddr, HDR_LEN_WITH_SECURITY_INFO, headerBuf);

    // First verify signerInfo
    verifyStatus = verifyCertElement(&headerBuf[SEG_SIGERINFO_OFFSET]);
    if(verifyStatus != SUCCESS)
    {
      return verifyStatus;
    }

    // Get the hash of the image
    uint8_t *finalHash = NULL;
    finalHash = computeSha2Hash(eflStartAddr, shaBuf, SHA_BUF_SZ, true);
    if(finalHash == NULL)
    {
      return FAIL;
    }

    // Verify the hash
    // Create temp buffer used for ECDSA sign verify, it should 6*ECDSA_KEY_LEN
    uint8_t tempWorkzone[ECDSA_SHA_TEMPWORKZONE_LEN];
    memset(tempWorkzone, 0, ECDSA_SHA_TEMPWORKZONE_LEN);

    verifyStatus = bimVerifyImage_ecc(_secureCertElement.certPayload.eccKey.pubKeyX,
                                      _secureCertElement.certPayload.eccKey.pubKeyY,
                                       finalHash,
                                       &headerBuf[SEG_SIGNR_OFFSET],
                                       &headerBuf[SEG_SIGNS_OFFSET],
                                       eccWorkzone,
                                       tempWorkzone);

    if(verifyStatus == SECURE_FW_ECC_STATUS_VALID_SIGNATURE)
    {
       verifyStatus = SUCCESS;
    }
    return verifyStatus;
}

#endif /*if defined(SECURITY) */
/*******************************************************************************
 * @fn          main
 *
 * @brief       C-code main function.
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 */
void main(void)
{
#ifdef FLASH_DEVICE_ERASE
    extFlashOpen();
    const ExtFlashInfo_t *pExtFlashInfo = extFlashInfo();
    
    // Read metadata page to see if it finds any starting from the first one
    int8_t response = isLastMetaData(0);
    if(response != EMPTY_METADATA)
    {
        // Erase flash
        extFlashErase(0, pExtFlashInfo->deviceSize);
        // Read metadata again
        response = isLastMetaData(0);
    }

#ifdef DEBUG        
    powerUpGpio();               
    if(response != EMPTY_METADATA)
    {
        lightRedLed();
    }
    else // Blink Green LED
    {
        blinkLed(GREEN_LED, 20, 50);
    }
    powerDownGpio();
#endif /* DEBUG */

    extFlashClose();
#endif /* FLASH_DEVICE_ERASE */
    
    Bim_checkImages();
}

/**************************************************************************************************
*/
