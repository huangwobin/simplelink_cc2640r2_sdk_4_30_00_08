/******************************************************************************

 @file  bsp_spi.c

 @brief Common API for SPI access. Driverlib implementation.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/ssi.h)
#include DeviceFamily_constructPath(driverlib/gpio.h)
#include DeviceFamily_constructPath(driverlib/prcm.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/rom.h)
#include DeviceFamily_constructPath(driverlib/ssi.h)

#include "bsp.h"
#include "bsp_spi.h"

/* Board specific settings for CC26xx SensorTag, PCB version 1.01
 *
 * Note that since this module is an experimental implementation,
 * board specific settings are directly hard coded here.
 */
#define BLS_SPI_BASE      SSI0_BASE
#define BLS_CPU_FREQ      48000000ul

/**
 * Write a command to SPI
 */
int bspSpiWrite(const uint8_t *buf, size_t len)
{
  while (len > 0)
  {
    uint32_t ul;

    SSIDataPut(BLS_SPI_BASE, *buf);
    ROM_SSIDataGet(BLS_SPI_BASE, &ul);
    len--;
    buf++;
  }

  return 0;
}

/**
 * Read from SPI
 */
int bspSpiRead(uint8_t *buf, size_t len)
{
  while (len > 0)
  {
    uint32_t ul;

    if (!ROM_SSIDataPutNonBlocking(BLS_SPI_BASE, 0))
    {
      /* Error */
      return -1;
    }

    ROM_SSIDataGet(BLS_SPI_BASE, &ul);
    *buf = (uint8_t) ul;
    len--;
    buf++;
  }

  return 0;
}


/* See bsp_spi.h file for description */
void bspSpiFlush(void)
{
  uint32_t ul;

  while (ROM_SSIDataGetNonBlocking(BLS_SPI_BASE, &ul));
}


/* See bsp_spi.h file for description */
void bspSpiOpen(uint32_t bitRate, uint32_t clkPin)
{
#ifdef BOOT_LOADER
  /* GPIO power && SPI power domain */
  PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL);
  while (PRCMPowerDomainStatus(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL)
         != PRCM_DOMAIN_POWER_ON);

  /* GPIO power */
  PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
  PRCMLoadSet();
  while (!PRCMLoadGet());
#endif

  /* SPI power */
  ROM_PRCMPeripheralRunEnable(PRCM_PERIPH_SSI0);
  PRCMLoadSet();
  while (!PRCMLoadGet());

  /* SPI configuration */
  SSIIntDisable(BLS_SPI_BASE, SSI_RXOR | SSI_RXFF | SSI_RXTO | SSI_TXFF);
  SSIIntClear(BLS_SPI_BASE, SSI_RXOR | SSI_RXTO);
  ROM_SSIConfigSetExpClk(BLS_SPI_BASE,
                         BLS_CPU_FREQ, /* CPU rate */
                         SSI_FRF_MOTO_MODE_0, /* frame format */
                         SSI_MODE_MASTER, /* mode */
                         bitRate, /* bit rate */
                         8); /* data size */
  ROM_IOCPinTypeSsiMaster(BLS_SPI_BASE, BSP_SPI_MISO, BSP_SPI_MOSI,
                          IOID_UNUSED /* csn */, clkPin);
  SSIEnable(BLS_SPI_BASE);

  {
    /* Get read of residual data from SSI port */
    uint32_t buf;

    while (SSIDataGetNonBlocking(BLS_SPI_BASE, &buf));
  }

}

/* See bsp_spi.h file for description */
void bspSpiClose(void)
{
  // Power down SSI0
  ROM_PRCMPeripheralRunDisable(PRCM_PERIPH_SSI0);
  PRCMLoadSet();
  while (!PRCMLoadGet());

#ifdef BOOT_LOADER
  PRCMPeripheralRunDisable(PRCM_PERIPH_GPIO);
  PRCMLoadSet();
  while (!PRCMLoadGet());

  PRCMPowerDomainOff(PRCM_DOMAIN_SERIAL | PRCM_DOMAIN_PERIPH);
  while (PRCMPowerDomainStatus(PRCM_DOMAIN_SERIAL | PRCM_DOMAIN_PERIPH)
         != PRCM_DOMAIN_POWER_OFF);
#endif
}
