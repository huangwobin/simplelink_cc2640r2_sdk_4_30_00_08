/******************************************************************************

 @file  bls_bsp.c

 @brief Base loader storage implementation for using board support package

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#include <stdint.h>
#include <stdlib.h>

#include "ext_flash.h"

#include "bls.h"

/* See bls.h file for description */
int BLS_init(void)
{
  if (extFlashOpen())
  {
    return 0;
  }
  return -1;
}

/* See bls.h file for description */
void BLS_close(void)
{
  extFlashClose();
}

/* See bls.h file for description */
int BLS_read(size_t offset, size_t length, uint8_t *buf)
{
  if (extFlashRead(offset, length, buf))
  {
    return 0;
  }
  return -1;
}

/* See bls.h file for description */
int BLS_write(size_t offset, size_t length, const uint8_t *buf)
{
  if (extFlashWrite(offset, length, buf))
  {
    return 0;
  }
  return -1;
}

/* See bls.h file for description */
int BLS_erase(size_t offset, size_t length)
{
  if (extFlashErase(offset, length))
  {
    return 0;
  }
  return -1;
}
