/******************************************************************************

 @file  hidvoiceservice.c

 @brief This file contains the HID service for a voice.

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2011-2020, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <icall.h>
#include "util.h"
/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"

#include "hidvoiceservice.h"
#include "hiddev.h"
#include "battservice.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// Number of HID reports defined in the service
#define HID_NUM_VOICE_REPORTS          2

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// HID Information characteristic value
static CONST uint8 hidInfo[HID_INFORMATION_LEN] =
{
  LO_UINT16(0x0111), HI_UINT16(0x0111),           // bcdHID (USB HID version)
  0x00,                                           // bCountryCode
  HID_VOICE_FLAGS                                 // Flags
};

// HID Report Map characteristic value
// Keyboard report descriptor (using format for Boot interface descriptor)
static CONST uint8 hidReportMap[] =
{
  0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined)
  0x09, 0x01,        // Usage (0x01)
  0xA1, 0x01,        // Collection (Application)
  0x85, HID_RPT_ID_VOICE_START_IN,        // Report ID

  0x09, 0x02,        //   Usage (0x02)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x04,        //   Report Count (4)
  0x81, 0x00,        //   Input: (Data,Ary,Abs)

  0xC0,               // End Collection

  0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined)
  0x09, 0x01,        // Usage (0x01)
  0xA1, 0x01,        // Collection (Application)
  0x85, HID_RPT_ID_VOICE_DATA_IN,        // Report ID

  0x09, 0x02,        //   Usage (0x02)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x14,        //   Report Count (20)
  0x81, 0x00,        //   Input: (Data,Ary,Abs)

  0xC0               // End Collection
};

// HID report mapping table
static hidRptMap_t  hidRptMap[HID_NUM_VOICE_REPORTS];

/*********************************************************************
 * Profile Attributes - variables
 */

// HID Service attribute
static CONST gattAttrType_t hidService = { ATT_BT_UUID_SIZE, hidServUUID };

// HID Information characteristic
static uint8 hidInfoProps = GATT_PROP_READ;

// HID Report Map characteristic
static uint8 hidReportMapProps = GATT_PROP_READ;

// HID External Report Reference Descriptor
static uint8 hidExtReportRefDesc[ATT_BT_UUID_SIZE] =
             { LO_UINT16(BATT_LEVEL_UUID), HI_UINT16(BATT_LEVEL_UUID) };

// HID Report characteristic, Voice Start
static uint8 hidReportVoiceStartProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8 hidReportVoiceStart;
static gattCharCfg_t *hidReportVoiceStartInClientCharCfg;
static uint8 hidReportRefVoiceStart[HID_REPORT_REF_LEN] =
             { HID_RPT_ID_VOICE_START_IN, HID_REPORT_TYPE_INPUT };

// HID Report characteristic, Voice Data
static uint8 hidReportVoiceDataProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8 hidReportVoiceData;
static gattCharCfg_t *hidReportVoiceDataInClientCharCfg;
static uint8 hidReportRefVoiceData[HID_REPORT_REF_LEN] =
             { HID_RPT_ID_VOICE_DATA_IN, HID_REPORT_TYPE_INPUT };

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t hidAttrTbl[] =
{
  // HID Service
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *) &hidService                     /* pValue */
  },

    // HID Information characteristic declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &hidInfoProps
    },

      // HID Information characteristic
      {
        { ATT_BT_UUID_SIZE, hidInfoUUID },
        GATT_PERMIT_ENCRYPT_READ,
        0,
        (uint8 *) hidInfo
      },

    // HID Report Map characteristic declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &hidReportMapProps
    },

      // HID Report Map characteristic
      {
        { ATT_BT_UUID_SIZE, hidReportMapUUID },
        GATT_PERMIT_ENCRYPT_READ,
        0,
        (uint8 *) hidReportMap
      },

      // HID External Report Reference Descriptor
      {
        { ATT_BT_UUID_SIZE, extReportRefUUID },
        GATT_PERMIT_READ,
        0,
        hidExtReportRefDesc
      },

    // HID Voice Start Input Report declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &hidReportVoiceStartProps
    },

      // HID Voice Start Input Report
      {
        { ATT_BT_UUID_SIZE, hidReportUUID },
        GATT_PERMIT_ENCRYPT_READ,
        0,
        &hidReportVoiceStart
      },

      // HID Voice Start Input Report characteristic client characteristic configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE,
        0,
        (uint8 *) &hidReportVoiceStartInClientCharCfg
      },

      // HID Report Reference characteristic descriptor, Voice Start
      {
        { ATT_BT_UUID_SIZE, reportRefUUID },
        GATT_PERMIT_READ,
        0,
        hidReportRefVoiceStart
      },

    // HID Voice Data Input Report declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &hidReportVoiceDataProps
    },

      // HID Voice Data Input Report
      {
        { ATT_BT_UUID_SIZE, hidReportUUID },
        GATT_PERMIT_ENCRYPT_READ,
        0,
        &hidReportVoiceData
      },

      // HID Voice Data Input Report characteristic client characteristic configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_ENCRYPT_WRITE,
        0,
        (uint8 *) &hidReportVoiceDataInClientCharCfg
      },

      // HID Report Reference characteristic descriptor, Voice Data
      {
        { ATT_BT_UUID_SIZE, reportRefUUID },
        GATT_PERMIT_READ,
        0,
        hidReportRefVoiceData
      },

};

// Attribute index enumeration-- these indexes match array elements above
enum
{
  HID_SERVICE_IDX,                // HID Service
  HID_INFO_DECL_IDX,              // HID Information characteristic declaration
  HID_INFO_IDX,                   // HID Information characteristic
  HID_REPORT_MAP_DECL_IDX,        // HID Report Map characteristic declaration
  HID_REPORT_MAP_IDX,             // HID Report Map characteristic
  HID_EXT_REPORT_REF_DESC_IDX,    // HID External Report Reference Descriptor
  HID_VOICE_START_IN_DECL_IDX,    // HID Voice Start Input Report declaration
  HID_VOICE_START_IN_IDX,         // HID Voice Start Input Report
  HID_VOICE_START_IN_CCCD_IDX,    // HID Voice Start Input Report characteristic client characteristic configuration
  HID_REPORT_REF_VOICE_START_IDX, // HID Report Reference characteristic descriptor, Voice Start
  HID_VOICE_DATA_IN_DECL_IDX,     // HID Voice Start Input Report declaration
  HID_VOICE_DATA_IN_IDX,          // HID Voice Start Input Report
  HID_VOICE_DATA_IN_CCCD_IDX,     // HID Voice Start Input Report characteristic client characteristic configuration
  HID_REPORT_REF_VOICE_DATA_IDX,  // HID Report Reference characteristic descriptor, Voice Start
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * PROFILE CALLBACKS
 */

// Service Callbacks
// Note: When an operation on a characteristic requires authorization and
// pfnAuthorizeAttrCB is not defined for that characteristic's service, the
// Stack will report a status of ATT_ERR_UNLIKELY to the client.  When an
// operation on a characteristic requires authorization the Stack will call
// pfnAuthorizeAttrCB to check a client's authorization prior to calling
// pfnReadAttrCB or pfnWriteAttrCB, so no checks for authorization need to be
// made within these functions.
CONST gattServiceCBs_t hidVoiceCBs =
{
  HidDev_ReadAttrCB,  // Read callback function pointer
  HidDev_WriteAttrCB, // Write callback function pointer
  NULL                // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      HidVoice_AddService
 *
 * @brief   Initializes the HID Service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   none
 *
 * @return  Success or Failure
 */
bStatus_t HidVoice_AddService(void)
{
  uint8 status = SUCCESS;

  // Allocate Client Charateristic Configuration tables.
  hidReportVoiceStartInClientCharCfg = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) *
                                                              linkDBNumConns);

  if (hidReportVoiceStartInClientCharCfg == NULL)
  {
    return ( bleMemAllocError );
  }

  hidReportVoiceDataInClientCharCfg = (gattCharCfg_t *)ICall_malloc(sizeof(gattCharCfg_t) *
                                                                  linkDBNumConns);

  if (hidReportVoiceDataInClientCharCfg == NULL)
  {
    ICall_free(hidReportVoiceStartInClientCharCfg);

    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg(INVALID_CONNHANDLE, hidReportVoiceStartInClientCharCfg);
  GATTServApp_InitCharCfg(INVALID_CONNHANDLE, hidReportVoiceDataInClientCharCfg);

  // Register GATT attribute list and CBs with GATT Server App
  status = GATTServApp_RegisterService(hidAttrTbl, GATT_NUM_ATTRS(hidAttrTbl),
                                       GATT_MAX_ENCRYPT_KEY_SIZE, &hidVoiceCBs);

  // Construct map of reports to characteristic handles
  // Each report is uniquely identified via its ID and type

  // Voice Start input report
  hidRptMap[0].id = hidReportRefVoiceStart[0];
  hidRptMap[0].type = hidReportRefVoiceStart[1];
  hidRptMap[0].handle = hidAttrTbl[HID_VOICE_START_IN_IDX].handle;
  hidRptMap[0].pCccdAttr = &hidAttrTbl[HID_VOICE_START_IN_CCCD_IDX];
  hidRptMap[0].mode = HID_PROTOCOL_MODE_REPORT;

  // Voice Data input report
  hidRptMap[1].id = hidReportRefVoiceData[0];
  hidRptMap[1].type = hidReportRefVoiceData[1];
  hidRptMap[1].handle = hidAttrTbl[HID_VOICE_DATA_IN_IDX].handle;
  hidRptMap[1].pCccdAttr = &hidAttrTbl[HID_VOICE_DATA_IN_CCCD_IDX];
  hidRptMap[1].mode = HID_PROTOCOL_MODE_REPORT;

  // Setup report ID map
  HidDev_RegisterReports(HID_NUM_VOICE_REPORTS, hidRptMap);

  return (status);
}

/*********************************************************************
 * @fn      HidVoice_SetParameter
 *
 * @brief   Set a HID Voice parameter.
 *
 * @param   id     - HID report ID.
 * @param   type   - HID report type.
 * @param   uuid   - attribute uuid.
 * @param   len    - length of data to right.
 * @param   pValue - pointer to data to write.  This is dependent on
 *          the input parameters and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  GATT status code.
 */
uint8 HidVoice_SetParameter(uint8 id, uint8 type, uint16 uuid, uint8 len,
                          void *pValue)
{
  bStatus_t ret = SUCCESS;

  switch (uuid)
  {
    case REPORT_UUID:
      if (type ==  HID_REPORT_TYPE_OUTPUT)
      {
        //nothing to do yet
      }
      else if (type == HID_REPORT_TYPE_FEATURE)
      {
        //nothing to do yet
      }
      else
      {
        ret = ATT_ERR_ATTR_NOT_FOUND;
      }
      break;

    default:
      // Ignore the request
      break;
  }

  return (ret);
}

/*********************************************************************
 * @fn      HidVoice_GetParameter
 *
 * @brief   Get a HID Voice parameter.
 *
 * @param   id     - HID report ID.
 * @param   type   - HID report type.
 * @param   uuid   - attribute uuid.
 * @param   pLen   - length of data to be read
 * @param   pValue - pointer to data to get.  This is dependent on
 *          the input parameters and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  GATT status code.
 */
uint8 HidVoice_GetParameter(uint8 id, uint8 type, uint16 uuid, uint8 *pLen,
                          void *pValue)
{
  switch (uuid)
  {
    case REPORT_UUID:
      if (type ==  HID_REPORT_TYPE_OUTPUT)
      {
        //nothing to do yet
      }
      else if (type == HID_REPORT_TYPE_FEATURE)
      {
        //nothing to do yet
      }
      else
      {
        *pLen = 0;
      }
      break;

    default:
      *pLen = 0;
      break;
  }

  return (SUCCESS);
}


/*********************************************************************
*********************************************************************/
