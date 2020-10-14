/******************************************************************************

 @file oad.h

 @brief Over the Air Download API

 Group: WCS LPC
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef OAD_H
#define OAD_H

/******************************************************************************
 Includes
 *****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 Typedefs
 *****************************************************************************/

/******************************************************************************
 Public Variables
 *****************************************************************************/

/*!
 * \ingroup Oad
 * @{
 */

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

/*!
 * @brief marks the external image for switch
 *
 * Sets the metadata for the factory image stored in external flash as ready to
 * run. Then resets the device to initiate the switch.
 *
 * @warn This function will not return if it succeeds
 */
extern void OAD_markSwitch(void);

/*! @} end group Oad */

#ifdef __cplusplus
}
#endif

#endif /* OAD_H */

