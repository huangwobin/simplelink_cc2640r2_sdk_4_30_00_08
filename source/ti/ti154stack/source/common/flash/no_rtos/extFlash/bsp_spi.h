/******************************************************************************

 @file  bsp_spi.h

 @brief Common API for SPI access

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
* Initialize SPI interface
*
* @return none
*/
extern void bspSpiOpen(uint32_t bitRate, uint32_t clkPin);

/**
* Close SPI interface
*
* @return True when successful.
*/
extern void bspSpiClose(void);

/**
* Clear data from SPI interface
*
* @return none
*/
extern void bspSpiFlush(void);

/**
* Read from an SPI device
*
* @return True when successful.
*/
extern int bspSpiRead( uint8_t *buf, size_t length);

/**
* Write to an SPI device
*
* @return True when successful.
*/
extern int bspSpiWrite(const uint8_t *buf, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SPI_H */
