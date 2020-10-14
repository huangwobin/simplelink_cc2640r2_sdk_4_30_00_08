/******************************************************************************

 @file  bls.h

 @brief Boot Loader storage abstraction

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef BLS_H
#define BLS_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * Initialize storage driver.
   *
   * @return Zero when successful.
   */
  extern int BLS_init(void);

  /**
   * Close the storage driver
   */
  extern void BLS_close(void);

  /**
   * Read storage content
   *
   * @return Zero when successful.
   */
  extern int BLS_read(size_t offset, size_t length, uint8_t *buf);

  /**
   * Erase storage sectors corresponding to the range.
   *
   * @return Zero when successful.
   */
  extern int BLS_erase(size_t offset, size_t length);

  /**
   * Write to storage sectors.
   *
   * @return Zero when successful.
   */
  extern int BLS_write(size_t offset, size_t length, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* BLS_H */
