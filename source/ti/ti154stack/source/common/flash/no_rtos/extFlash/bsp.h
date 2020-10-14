/******************************************************************************

 @file  bsp.h

 @brief Board support package header file for CC2650 LaunchPad

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef __BSP_H__
#define __BSP_H__


/******************************************************************************
* If building with a C++ compiler, make all of the definitions in this header
* have a C binding.
******************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif


/******************************************************************************
* INCLUDES
*/
#include <stdint.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_sysctl.h) // Access to the GET_MCU_CLOCK define
#include DeviceFamily_constructPath(inc/hw_ioc.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/gpio.h)

/******************************************************************************
* DEFINES
*/

#ifdef CC1350STK
// Board LED defines
#define BSP_IOID_LED_1          IOID_10
#define BSP_IOID_LED_2          IOID_10

// Board key defines
#define BSP_IOID_KEY_LEFT       IOID_15
#define BSP_IOID_KEY_RIGHT      IOID_4

// Board external flash defines
#define BSP_IOID_FLASH_CS       IOID_14
#define BSP_SPI_MOSI            IOID_19
#define BSP_SPI_MISO            IOID_18
#define BSP_SPI_CLK_FLASH       IOID_17
#else
// Board LED defines
#define BSP_IOID_LED_1          IOID_6
#define BSP_IOID_LED_2          IOID_7

// Board key defines
#define BSP_IOID_KEY_LEFT       IOID_13
#define BSP_IOID_KEY_RIGHT      IOID_14

// Board external flash defines
#define BSP_IOID_FLASH_CS       IOID_20
#define BSP_SPI_MOSI            IOID_9
#define BSP_SPI_MISO            IOID_8
#define BSP_SPI_CLK_FLASH       IOID_10
#endif
/******************************************************************************
* Mark the end of the C bindings section for C++ compilers.
******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __BSP_H__ */
