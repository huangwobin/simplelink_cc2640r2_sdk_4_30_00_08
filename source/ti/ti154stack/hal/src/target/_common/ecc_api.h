/******************************************************************************

 @file  ecc_api.h

 @brief Header for ECC proxy for stack's interface to the ECC driver.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef ECC_API_H
#define ECC_API_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "ecc/ECCROMCC26XX.h"

extern uint32_t *eccDrvTblPtr;

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

// ECC proxy index for ECC driver API
#define ECC_INIT                        0
#define ECC_INIT_PARAMS                 1
#define ECC_GEN_KEYS                    2
#define ECC_GEN_DHKEY                   3

/*
** ECC API Proxy
*/

#define ECC_TABLE( index )   (*((uint32_t *)((uint32_t)eccDrvTblPtr + (uint32_t)((index)*4))))

#define ECC_init        ((void (*)(void))                                                                         ECC_TABLE(ECC_INIT))
#define ECC_Params_init ((void (*)(ECCROMCC26XX_Params *))                                                        ECC_TABLE(ECC_INIT_PARAMS))
#define ECC_genKeys     ((int8 (*)(uint8_t *, uint8_t *, uint8_t *, ECCROMCC26XX_Params *))                       ECC_TABLE(ECC_GEN_KEYS))
#define ECC_genDHKey    ((int8 (*)(uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *, ECCROMCC26XX_Params *)) ECC_TABLE(ECC_GEN_DHKEY))

#ifdef __cplusplus
}
#endif

#endif /* ECC_API_H */
