/******************************************************************************

 @file  hal_assert.c

 @brief Abort implementation for service implementation environment.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#include <icall.h>
#include "osal.h"

void halAssertHandler(void)
{
  ICall_abort();
}
