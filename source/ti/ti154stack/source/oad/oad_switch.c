/******************************************************************************

 @file oad.c

 @brief Over the Air Download for use with the BLE OAD example

 Group: WCS LPC
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/******************************************************************************
 Includes
 *****************************************************************************/
#if defined(FEATURE_BLE_OAD)
#include <string.h>
#include <stdint.h>
#include <sys_ctrl.h>
#include <extflash/ExtFlash.h>
#include "common/native_oad/ext_flash_layout.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* dummy image metadata */
#if defined (__IAR_SYSTEMS_ICC__)
#pragma location=".checksum"
/* 4 bytes for CRC and CRC Shadow. */
const uint8_t _chksum[4] =
{
    0xFF, 0xFF, 0xFF, 0xFF
};
/* 12 byte for image header immediately following CRC/CRC Shadow. */
#pragma location="IMAGE_HEADER"
const uint8_t _imgHdr[12] =
{
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF
};
#elif defined __TI_COMPILER_VERSION__
#pragma DATA_SECTION(_imgHdr, ".imgHdr")
#pragma RETAIN(_imgHdr)
/*
 For CCS, this is the first 16 bytes at a 0 byte offset from the start of the
 OAD Image.
 */
const uint8_t _imgHdr[16] =
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF };
#endif

/******************************************************************************
 External variables
 *****************************************************************************/

/******************************************************************************
 Public variables
 *****************************************************************************/

/******************************************************************************
 Local variables
 *****************************************************************************/

ExtImageInfo_t imgInfo;

/******************************************************************************
 Local function prototypes
 *****************************************************************************/

/******************************************************************************
 Public Functions
 *****************************************************************************/

/*!
 Marks the image in external flash as ready to run

 Public function defined in oad.h
 */
void OAD_markSwitch(void)
{
    if(!ExtFlash_open())
    {
        return;
    }

    /*
     Read the second image metadata from external flash. We assume that the
     user has loaded a full image into the second image slot. The factory
     image concept has not been implemented in the BLE OAD example as of this
     writing.
     */
    ExtFlash_read(EFL_IMAGE_INFO_ADDR_BLE, sizeof(ExtImageInfo_t),
                  (uint8_t *) &imgInfo);

    if((imgInfo.crc[0] == imgInfo.crc[1]) &&
            (imgInfo.imgType == EFL_OAD_IMG_TYPE_STACK))
    {
        /*
         Mark the image as ready to run if the crc is valid and pointing to
         the second image slot, then reset to let the BIM load the new image.
         */
        ExtFlash_erase(EFL_IMAGE_INFO_ADDR_BLE, EFL_PAGE_SIZE);
        imgInfo.status = 0xFF;
        ExtFlash_write(EFL_IMAGE_INFO_ADDR_BLE, sizeof(ExtImageInfo_t),
                       (uint8_t *) &imgInfo);

        SysCtrlSystemReset();
    }

    /*
     We did not find a valid image, close the driver and return.
     */
    ExtFlash_close();
    return;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/

#endif /* FEATURE_BLE_OAD */
