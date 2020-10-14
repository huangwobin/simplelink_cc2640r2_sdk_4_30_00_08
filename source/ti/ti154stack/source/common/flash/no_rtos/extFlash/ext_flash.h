/******************************************************************************

 @file  ext_flash.h

 @brief External flash storage abstraction.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef EXT_FLASH_H
#define EXT_FLASH_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define EXT_FLASH_PAGE_SIZE   4096

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint32_t deviceSize; // bytes
    uint8_t manfId;      // manufacturer ID
    uint8_t devId;       // device ID
} ExtFlashInfo_t;

/**
* Initialize storage driver.
*
* @return True when successful.
*/
extern bool extFlashOpen(void);

/**
* Close the storage driver
*/
extern void extFlashClose(void);

/**
* Get flash information
*/
extern const ExtFlashInfo_t *extFlashInfo(void);

/**
* Read storage content
*
* @return True when successful.
*/
extern bool extFlashRead(size_t offset, size_t length, uint8_t *buf);

/**
* Erase storage sectors corresponding to the range.
*
* @return True when successful.
*/
extern bool extFlashErase(size_t offset, size_t length);

/**
* Write to storage sectors.
*
* @return True when successful.
*/
extern bool extFlashWrite(size_t offset, size_t length, const uint8_t *buf);

/**
* Test the flash (power on self-test)
*
* @return True when successful.
*/
extern bool extFlashTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EXT_FLASH_H */
