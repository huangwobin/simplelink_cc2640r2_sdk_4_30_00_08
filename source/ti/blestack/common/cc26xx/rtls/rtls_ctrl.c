/******************************************************************************

 @file  rtls_ctrl.c

 @brief This file contains all functions and definitions related to RTLS Control

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2018-2020, Texas Instruments Incorporated
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

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>

#include <string.h>
#include <stdlib.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <driverlib/sys_ctrl.h>

#include "util.h"

#include "rtls_host.h"
#include "rtls_ctrl.h"
#include "rtls_ctrl_aoa.h"
#include "rtls_ctrl_api.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// RTLS Control Remote Command to Slave opcodes
#define RTLS_REMOTE_CMD_RESERVED1          0x01
#define RTLS_REMOTE_CMD_RESERVED2          0x02
#define RTLS_REMOTE_CMD_RESERVED3          0x03
#define RTLS_REMOTE_CMD_RESERVED4          0x04
#define RTLS_REMOTE_CMD_RESERVED5          0x05

// The maximum value for alpha in the RSSI filter
#define RTLS_CTRL_ALPHA_FILTER_MAX_VALUE  16

// The larger this number is, the effect which the last
// sample will have on RSSI is greater
#define RTLS_CTRL_ALPHA_FILTER_VALUE      4

// Initial RSSI value for the alpha filter (first dummy sample)
#define RTLS_CTRL_FILTER_INITIAL_RSSI     -55

// RSSI check
#define RTLS_IS_VALID_RSSI(rssi)          ((rssi) < 127 && (rssi > -127))

// Max string length on debug event
#define DEBUG_STRING_SIZE       64

/*********************************************************************
 * TYPEDEFS
 */

// RTLS Control states
typedef enum
{
  RTLS_STATE_CONNECTED          = 0x00000001,
  RTLS_STATE_RESERVED           = 0x00000002,
  RTLS_STATE_AOA_ENABLED        = 0x00000004,
  RTLS_STATE_CONN_INFO_ENABLED  = 0x00000008,
} rtlsConnState_e;

// RSSI alpha filter structure
typedef struct
{
  int8_t currentRssi;
  uint8_t alphaValue;
} rssiAlphaFilter_t;

// RTLS Run Event
typedef struct
{
  uint8_t status;
  uint32_t timeToNextEvent;
  int8_t rssi;
  uint8_t channel;
} rtlsRunEvt_t;

// RTLS Connection Info Event
typedef struct __attribute__((packed))
{
  int8_t rssi;
  uint8_t channel;
} rtlsConnInfoEvt_t;

// RTLS Control Data Structures
typedef struct __attribute__((packed))
{
  rtlsCapabilities_e capab;             // Capabilities
  uint16_t revNum;                      // Revision
  uint8_t devId;                        // Device ID
  uint8_t identifier[CHIP_ID_SIZE];     // Unique identifier
} rtlsCapabilities_t;

// General data structure used for various RTLS Control operations
typedef struct
{
  pfnRtlsAppCb appCb;                   // RTLS Control callback to RTLS Application
  rtlsAoa_t aoaControlBlock;            // This contains all AoA information
  rtlsConnState_e connStateBm;          // State of the connection managed by the RTLS Application
  rtlsCapabilities_t rtlsCapab;         // Capabilities of the device
  rssiAlphaFilter_t rssiFilter;         // RSSI value gathered from different sources
} rtlsCtrlData_t;

// RTLS Control message types
typedef enum
{
  HOST_MSG_EVENT,
  RTLS_RUN_EVENT,
  AOA_RESULTS_EVENT
} rtlsEvtType_e;

// RTLS Control RTOS Events
typedef struct
{
  rtlsEvtType_e event; // Event Id
  uint8_t *pData;      // Pointer to the data
} rtlsEvt_t;

typedef struct __attribute__((packed))
{
  uint32_t debug_value;
  uint8_t  debug_string[DEBUG_STRING_SIZE];
} debugInfo_t;

// RTLS param Event
typedef struct __attribute__((packed))
{
  uint8_t rtlsParamType;
  uint8_t status;
} setRtlsParamResponse_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */
rtlsCtrlData_t gRtlsData =
{
  .appCb                    = NULL,
  .aoaControlBlock          = {0},
  .connStateBm              = (rtlsConnState_e)0x00000000,
  .rtlsCapab.capab          = RTLS_CAP_NOT_INITIALIZED,
  .rtlsCapab.identifier     = {0},
  .rssiFilter               = {0}
};

/*********************************************************************
 * LOCAL VARIABLES
 */
// Event globally used to post local events and pend on local events
Event_Handle syncRtlsEvent;

// Queue object used for app messages
Queue_Struct rtlsCtrlMsg;
Queue_Handle rtlsCtrlMsgQueue;

// Task configuration
Task_Struct rtlsTask;
Char rtlsTaskStack[RTLS_CTRL_TASK_STACK_SIZE];

/*********************************************************************
 * LOCAL FUNCTIONS
 */

// RTLS Control specific
void RTLSCtrl_createTask(void);
void RTLSCtrl_taskFxn(UArg a0, UArg a1);
void RTLSCtrl_processMessage(rtlsEvt_t *pMsg);
void RTLSCtrl_enqueueMsg(uint16_t eventId, uint8_t *pMsg);

// Host Command Handlers
void RTLSCtrl_connReqCmd(uint8_t *connParams);
void RTLSCtrl_scanReqCmd(void);
void RTLSCtrl_sendRtlsRemoteCmd(uint8_t cmdOp, uint8_t *pData, uint16_t dataLen);
void RTLSCtrl_terminateLinkCmd(void);
void RTLSCtrl_enableConnInfoCmd(uint8_t *enableConnInfoCmd);
void RTLSCtrl_setAoaParamsCmd(uint8_t *pParams);
void RTLSCtrl_sendSlaveAoaParamsCmd(uint8_t pendingParams);
void RTLSCtrl_enableAoaCmd(uint8_t *enableAoaCmd);

// Internal functions
void RTLSCtrl_processHostMessage(rtlsHostMsg_t *pHostMsg);
void RTLSCtrl_hostMsgCB(rtlsHostMsg_t *pMsg);
void RTLSCtrl_callRtlsApp(uint8_t reqOp, uint8_t *data);
rtlsStatus_e RTLSCtrl_processSyncEvent(uint8_t *pMsg);
rtlsStatus_e RTLSCtrl_updateConnState(rtlsConnState_e connState, uint8_t enableDisableFlag);
rtlsStatus_e RTLSCtrl_updateConnInterval(uint8_t *msg);

// Board specific
void RTLSCtrl_resetDevice(void);
void RTLSCtrl_initAntenna(uint8_t pin, uint8_t pinState);

// RSSI Trigger specific
void RTLSCtrl_calculateRSSI(int lastRssi);

/*********************************************************************
 * EXTERN FUNCTIONS
 */

/*********************************************************************
 * @fn      RTLSCtrl_open
 *
 * @design /ref 159098678
 *
 * @brief   RTLS Initialization function from application side
 *          Application needs to specify the mandatory function
 *          required by RTLS Control
 *
 * @param   appCBs - Struct already filled by the application
 * @param   ctrlCBs - Will be filled by RTLS Control
 *
 * @return  none
 */
void RTLSCtrl_open(rtlsConfiguration_t *rtlsConfig)
{
  // Open Host I/F
  RTLSHost_openHostIf(&RTLSCtrl_hostMsgCB);

  gRtlsData.appCb = rtlsConfig->rtlsAppCb;
  gRtlsData.rtlsCapab.devId = rtlsConfig->devId;
  gRtlsData.rtlsCapab.revNum = rtlsConfig->revNum;
  gRtlsData.rtlsCapab.capab = rtlsConfig->rtlsCapab;

  memcpy(gRtlsData.rtlsCapab.identifier, rtlsConfig->identifier, CHIP_ID_SIZE);

  // Initialize a pin out of BOOSTXL-AOA pins to act as an antenna
  // When BOOSTXL-AOA is not present a single pin will be set to high
  // We will be using pin id 28 to act as an initial antenna
  RTLSCtrl_initAntenna(28, 1);

  // Create RTLS Control task
  RTLSCtrl_createTask();
}

/*********************************************************************
 * @fn      RTLSCtrl_scanResultEvt
 *
 * @design /ref 159098678
 *
 * @brief   Application will call this function once scan results have
 *          been collected
 *
 * @param   scanResults - Pointer to the scan results
 * @param   size - size of scanResult pointer
 *
 * @return  none
 */
void RTLSCtrl_scanResultEvt(rtlsStatus_e status, uint8_t *scanResult, uint8_t size)
{
  rtlsStatus_e scanStatus = status;

  if (scanResult == NULL)
  {
    RTLSHost_sendMsg(RTLS_CMD_SCAN_STOP, HOST_ASYNC_RSP, (uint8_t *)&scanStatus, sizeof(uint8_t));
  }
  else
  {
    RTLSHost_sendMsg(RTLS_CMD_SCAN, HOST_ASYNC_RSP, scanResult, size);
  }
}

/*********************************************************************
 * @fn      RTLSCtrl_connResultEvt
 *
 * @design /ref 159098678
 *
 * @brief   RTLS Control will use this function to notify Node Manager that
 *          a connection has been formed
 *
 * @param   status - Connection successful or not
 *
 * @return  none
 */
void RTLSCtrl_connResultEvt(uint8_t status)
{
  if (status == RTLS_SUCCESS)
  {
    gRtlsData.connStateBm |= RTLS_STATE_CONNECTED;
  }
  else
  {
    // We were disconnected, if AOA is enabled we should disable it
    if (gRtlsData.connStateBm & RTLS_STATE_AOA_ENABLED)
    {
      uint8_t *syncReq;

      if ((syncReq = (uint8_t *)RTLSCtrl_malloc(sizeof(uint8_t))) == NULL)
      {
        // We failed to allocate, host was already notified, just exit
        return;
      }

      *syncReq = RTLS_FALSE;

      // Disable sync events
      RTLSCtrl_callRtlsApp(RTLS_REQ_ENABLE_SYNC, syncReq);
    }

    gRtlsData.connStateBm = (rtlsConnState_e)0;
  }

  RTLSHost_sendMsg(RTLS_CMD_CONNECT, HOST_ASYNC_RSP, &status, sizeof(status));
}

/*********************************************************************
 * @fn      RTLSCtrl_connInfoEvt
 *
 * @design /ref 159098678
 *
 * @brief   This function will send out connection information to Node Manager
 *
 * @param   connInfo - Connection information
 * @param   connInfoLen - Length of connInfo array
 *
 * @return  none
 */
void RTLSCtrl_connInfoEvt(uint8_t *connInfo, uint16_t connInfoLen)
{
  RTLSHost_sendMsg(RTLS_CMD_CONN_PARAMS, HOST_ASYNC_RSP, connInfo, connInfoLen);
}

/*********************************************************************
 * @fn      RTLSCtrl_syncNotifyEvt
 *
 * @design /ref 159098678
 *
 * @brief   Application will call this function on each sync event
 *
 * @param   status - the status of the sync event (tells us if the RF has received a sync packet)
 * @param   timeToNextEvent - the time to the next sync event
 * @param   rssi - current rssi at the time of the sync event
 * @param   channel - channel on which the sync event was received
 *
 * @return  none
 */
void RTLSCtrl_syncNotifyEvt(rtlsStatus_e status, uint32_t timeToNextEvent, int8_t rssi, uint8_t channel)
{
  rtlsRunEvt_t *pMsg;

  if ((pMsg = (rtlsRunEvt_t *)RTLSCtrl_malloc(sizeof(rtlsRunEvt_t))) == NULL)
  {
    // We failed to allocate, host was already notified, just exit
    return;
  }

  pMsg->status = status;
  pMsg->timeToNextEvent = timeToNextEvent;
  pMsg->rssi = rssi;
  pMsg->channel = channel;

  RTLSCtrl_enqueueMsg(RTLS_RUN_EVENT, (uint8_t *)pMsg);
}

/*********************************************************************
 * @fn      RTLSCtrl_dataSentEvt
 *
 * @design /ref 159098678
 *
 * @brief   This function is used by the RTLS Application to report the status of a sent packet
 *
 * @param   status - Packet transmitted/failed
 *
 * @return  none
 */
void RTLSCtrl_dataSentEvt(uint8_t status)
{
}

/*********************************************************************
 * @fn      RTLSCtrl_processSyncEvent
 *
 * @design /ref 159098678
 *
 * @brief   Application will call this function on each sync event
 *
 * @param   timeToNextEvent - the time to the next sync event
 *
 * @return  RTLS status
 */
rtlsStatus_e RTLSCtrl_processSyncEvent(uint8_t *pMsg)
{
  rtlsRunEvt_t *runEvt = (rtlsRunEvt_t *)pMsg;

  // Sanity check
  if (pMsg == NULL)
  {
    return RTLS_FAIL;
  }

  if (RTLS_IS_VALID_RSSI(runEvt->rssi))
  {
    RTLSCtrl_calculateRSSI(runEvt->rssi);

    if (gRtlsData.connStateBm & RTLS_STATE_CONN_INFO_ENABLED)
    {
        rtlsConnInfoEvt_t connInfoEvt;
        connInfoEvt.rssi = runEvt->rssi;
        connInfoEvt.channel = runEvt->channel;

        RTLSHost_sendMsg(RTLS_EVT_CONN_INFO, HOST_ASYNC_RSP, (uint8_t *)&connInfoEvt, sizeof(rtlsConnInfoEvt_t));
    }
  }

#if defined(RTLS_PASSIVE)
  // Only RTLS Passive does AoA post process at this point
  if ((gRtlsData.connStateBm & RTLS_STATE_AOA_ENABLED) && (gRtlsData.rtlsCapab.capab & RTLS_CAP_AOA_RX))
  {
    RTLSCtrl_postProcessAoa(&gRtlsData.aoaControlBlock, runEvt->rssi, runEvt->channel);
  }
#endif

    return (RTLS_SUCCESS);
}

/*********************************************************************
 * @fn      RTLSCtrl_scanReqCmd
 *
 * @design /ref 159098678
 *
 * @brief   Handles a scan request from RTLS Node Manager
 *          Once the request is received, RTLS Control will call the
 *          registered application's scan function and notify Node Manager
 *          that a scan has started
 *
 * @param   none
 *
 * @return  none
 */
void RTLSCtrl_scanReqCmd(void)
{
  rtlsStatus_e status = RTLS_SUCCESS;

  RTLSHost_sendMsg(RTLS_CMD_SCAN, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(uint8_t));

  RTLSCtrl_callRtlsApp(RTLS_REQ_SCAN, NULL);
}

/*********************************************************************
 * @fn      RTLSCtrl_connReqCmd
 *
 * @design /ref 159098678
 *
 * @brief   RTLS Control will use this function when Node Manager asks to
 *          form a connection
 *
 * @param   connParams - Pointer to connection parameters
 *
 * @return  none
 */
void RTLSCtrl_connReqCmd(uint8_t *connParams)
{
  rtlsStatus_e status = RTLS_SUCCESS;

  if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_MASTER)
  {
    RTLSHost_sendMsg(RTLS_CMD_CONNECT, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
  }
  else if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_PASSIVE)
  {
    // For a connection monitor the command is a bit different since it contains not only
    // the address to connect to but also different stack specific parameters that allow tracking
    RTLSHost_sendMsg(RTLS_CMD_CONN_PARAMS, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
  }

  RTLSCtrl_callRtlsApp(RTLS_REQ_CONN, connParams);
}

/*********************************************************************
 * @fn      RTLSCtrl_updateConnInterval
 *
 * @design /ref 159098678
 *
 * @brief   RTLS Control will use this function when Node Manager asks to
 *          dynamically change the connection interval
 *
 * @param   msg - Pointer to connection interval
 *
 * @return  rtlsStatus_e
 */
rtlsStatus_e RTLSCtrl_updateConnInterval(uint8_t *msg)
{
  rtlsStatus_e status = RTLS_FAIL;

  if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_MASTER)
  {
    if (gRtlsData.connStateBm & RTLS_STATE_CONNECTED)
    {
      RTLSCtrl_callRtlsApp(RTLS_REQ_UPDATE_CONN_INTERVAL, msg);
      status = RTLS_SUCCESS;
    }
  }
  
  return status;
}

/*********************************************************************
 * @fn      RTLSCtrl_terminateLinkCmd
 *
 * @design /ref 159098678
 *
 * @brief   Terminate active link
 *
 * @param   none
 *
 * @return  none
 */
void RTLSCtrl_terminateLinkCmd(void)
{
  rtlsStatus_e status = RTLS_SUCCESS;

  RTLSHost_sendMsg(RTLS_CMD_TERMINATE_LINK, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(status));

  RTLSCtrl_callRtlsApp(RTLS_REQ_TERMINATE_LINK, NULL);
}

/*********************************************************************
 * @fn      RTLSCtrl_enableConnInfoCmd
 *
 * @design /ref 159098678
 *
 * @brief   Enable report of RSSI and channels of BLE connection
 *
 * @param   enableConnInfoCmd - enable parameters
 *
 * @return  none
 */
void RTLSCtrl_enableConnInfoCmd(uint8_t *enableConnInfoCmd)
{
  rtlsStatus_e status = RTLS_SUCCESS;

  // Sanity check
  if (enableConnInfoCmd == NULL)
  {
    status = RTLS_FAIL;
    RTLSHost_sendMsg(RTLS_CMD_CONN_INFO, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
    return;
  }
  
  // Update RTLS connection state and enable/disable sync event if needed
  if (RTLSCtrl_updateConnState((rtlsConnState_e)RTLS_STATE_CONN_INFO_ENABLED, *enableConnInfoCmd) == RTLS_FAIL)
  {
    // We failed to allocate, host was already notified, just exit
    return;
  }

  RTLSHost_sendMsg(RTLS_CMD_CONN_INFO, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
}
/*********************************************************************
 * @fn      RTLSCtrl_updateConnState
 *
 * @design /ref 159098678
 *
 * @brief   Update RTLS connection state and enable/disable sync event
 *
 * @param   connState - RTLS control state
 *
 * @param   enableDisableFlag - Enable/disable connState
 *
 * @return  RTLS status
 */
rtlsStatus_e RTLSCtrl_updateConnState(rtlsConnState_e connState, uint8_t enableDisableFlag)
{
  uint8_t *syncReq;
  uint8_t updateSyncFlag = 0;

  // Enable RTLS control state
  if (enableDisableFlag == RTLS_TRUE)
  {
    // First connection available
    if (gRtlsData.connStateBm == RTLS_STATE_CONNECTED)
    {
        updateSyncFlag = 1;
    }
    gRtlsData.connStateBm |= connState;
  }
  // Disable RTLS control state
  else
  {
    gRtlsData.connStateBm &= ~(connState);
    // Last connection available
    if (gRtlsData.connStateBm <= RTLS_STATE_CONNECTED)
    {
        updateSyncFlag = 1;
    }
  }

  if (updateSyncFlag == 1)
  {
    if ((syncReq = (uint8_t *)RTLSCtrl_malloc(sizeof(uint8_t))) == NULL)
    {
      // We failed to allocate, host was already notified, just exit
      return RTLS_FAIL;
    }

    *syncReq = enableDisableFlag;

    // Enable/disable sync events
    RTLSCtrl_callRtlsApp(RTLS_REQ_ENABLE_SYNC, syncReq);
  }

  return RTLS_SUCCESS;
}

/*********************************************************************
 * @fn      RTLSCtrl_resetDevice
 *
 * @design /ref 159098678
 *
 * @brief   Resets device
 *
 * @param   none
 *
 * @return  none
 */
void RTLSCtrl_resetDevice(void)
{
  SysCtrlSystemReset();
}

/*********************************************************************
 * @fn      RTLSCtrl_setAoaParamsCmd
 *
 * @design /ref 159098678
 *
 * @brief   Handle configuring AoA parameters
 *
 * @param   pParams - AoA parameters
 *
 * @return  none
 */
void RTLSCtrl_setAoaParamsCmd(uint8_t *pParams)
{
  rtlsAoaParams_t *pAoaParams;
  rtlsStatus_e status = RTLS_SUCCESS;
  uint8_t startAntenna = 1;

  pAoaParams = (rtlsAoaParams_t *)pParams;

  memcpy(&gRtlsData.aoaControlBlock.aoaParams, pAoaParams, sizeof(rtlsAoaParams_t));

  if (IS_AOA_CONFIG_ONLY_ANT_2(gRtlsData.aoaControlBlock.aoaParams.sampleCtrl))
  {
    startAntenna = 2;
  }
  RTLSCtrl_initAoa(startAntenna);

  RTLSHost_sendMsg(RTLS_CMD_AOA_SET_PARAMS, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
}

/*********************************************************************
 * @fn      RTLSCtrl_enableAoaCmd
 *
 * @design /ref 159098678
 *
 * @brief   Enable master and slave
 *
 * @param   enableAoaCmd - enable parameters
 *
 * @return  none
 */
void RTLSCtrl_enableAoaCmd(uint8_t *enableAoaCmd)
{
  rtlsEnableAoaCmd_t *pCmd = (rtlsEnableAoaCmd_t *)enableAoaCmd;
  rtlsStatus_e status = RTLS_SUCCESS;

  // Sanity check
  if (enableAoaCmd == NULL)
  {
    if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_MASTER || gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_PASSIVE)
    {
      status = RTLS_FAIL;
      RTLSHost_sendMsg(RTLS_CMD_AOA_ENABLE, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
    }
    return;
  }

  if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_MASTER || gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_PASSIVE)
  {
    RTLSHost_sendMsg(RTLS_CMD_AOA_ENABLE, HOST_SYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
  }

#if defined(RTLS_LOCATIONING_AOA) && ((RTLS_PASSIVE) || (RTLS_SLAVE))

  if (gRtlsData.rtlsCapab.capab & RTLS_CAP_RTLS_PASSIVE)
  {
    // Update RTLS connection state and enable/disable sync event if needed
    if (RTLSCtrl_updateConnState((rtlsConnState_e)RTLS_STATE_AOA_ENABLED, pCmd->enableAoa) == RTLS_FAIL)
    {
      // We failed to allocate, host was already notified, just exit
      return;
    }
  }

  if (pCmd->enableAoa == RTLS_TRUE)
  {
    gRtlsData.connStateBm |= RTLS_STATE_AOA_ENABLED;

    RTLSCtrl_aoaEnable(&gRtlsData.aoaControlBlock);
  }
  else
  {
    gRtlsData.connStateBm &= ~(RTLS_STATE_AOA_ENABLED);

    RTLSCtrl_aoaDisable();
  }

#endif
}

/*********************************************************************
 * @fn      RTLSCtrl_callRtlsApp
 *
 * @design /ref 159098678
 *
 * @brief   Calls the callback provided by the RTLS Application
 *
 * @param   cmdOp - command to be executed
 * @param   data  - pointer to the data
 *
 * @return  none
 */
void RTLSCtrl_callRtlsApp(uint8_t reqOp, uint8_t *data)
{
  rtlsCtrlReq_t *appReq;

  if ((appReq = (rtlsCtrlReq_t *)RTLSCtrl_malloc(sizeof(rtlsCtrlReq_t))) == NULL)
  {
    // We failed to allocate, host was already notified, just exit
    return;
  }

  appReq->reqOp = reqOp;
  appReq->pData = data;

  if (gRtlsData.appCb != NULL)
  {
    gRtlsData.appCb((uint8_t *)appReq);
  }
}

/*********************************************************************
 * @fn      RTLSCtrl_sendRtlsRemoteCmd
 *
 * @design /ref 159098678
 *
 * @brief   Send a command to RTLS Slave
 *
 * @param   cmdOp - command to be executed
 * @param   data  - pointer to the data
 * @param   dataLen - length of data to send
 *
 * @return  none
 */
void RTLSCtrl_sendRtlsRemoteCmd(uint8_t cmdOp, uint8_t *pData, uint16_t dataLen)
{
  rtlsPacket_t *pRemoteCmd;

  // Create the RTLS remote command
  if ((pRemoteCmd = (rtlsPacket_t *)RTLSCtrl_malloc(sizeof(rtlsPacket_t) + dataLen)) == NULL)
  {
    // We failed to allocate, host was already notified, just exit
    return;
  }

  pRemoteCmd->cmdOp = cmdOp;
  pRemoteCmd->payloadLen = sizeof(rtlsPacket_t) + dataLen;
  memcpy(pRemoteCmd->pPayload, pData, dataLen);

  RTLSCtrl_callRtlsApp(RTLS_REQ_SEND_DATA, (uint8_t *)pRemoteCmd);
}

/*********************************************************************
 * @fn      RTLSCtrl_rtlsPacketEvt
 *
 * @design /ref 159098678
 *
 * @brief   Process an incoming RTLS packet (contains a remote command)
 *          Used only by devices NOT connected via uNPI (RTLS Slave)
 *
 * @param   pPkt - Pointer to the packet
 *
 * @return  none
 */
void RTLSCtrl_rtlsPacketEvt(uint8_t *pPkt)
{
  rtlsPacket_t *pRtlsPkt = (rtlsPacket_t*)pPkt;

  switch (pRtlsPkt->cmdOp)
  {
    // Add processing of opcodes here
  }

  RTLSUTIL_FREE(pRtlsPkt);
}

/*********************************************************************
 * @fn      RTLSCtrl_processHostMessage
 *
 * @design /ref 159098678
 *
 * @brief   Process an incoming message to RTLS Control
 *
 * @param   pHostMsg - pointer to the message
 *
 * @return  none
 */
void RTLSCtrl_processHostMessage(rtlsHostMsg_t *pHostMsg)
{
  // Note that messages that stop in this module should be freed here
  // Messages that are passed to the application should NOT be freed here, they are freed by the receiver
  // Messages that do not have payload are not freed either
  if (pHostMsg->cmdType == HOST_SYNC_REQ)
  {
    switch(pHostMsg->cmdId)
    {
      case RTLS_CMD_IDENTIFY:
      {
        RTLSHost_sendMsg(RTLS_CMD_IDENTIFY, HOST_SYNC_RSP, (uint8_t *)&gRtlsData.rtlsCapab, sizeof(rtlsCapabilities_t));
      }
      break;

      case RTLS_CMD_SCAN:
      {
        RTLSCtrl_scanReqCmd();
      }
      break;

      case RTLS_CMD_CONNECT:
      {
        RTLSCtrl_connReqCmd(pHostMsg->pData);
      }
      break;

      case RTLS_CMD_TERMINATE_LINK:
      {
        RTLSCtrl_terminateLinkCmd();
      }
      break;

      case RTLS_CMD_RESET_DEVICE:
      {
        RTLSCtrl_resetDevice();
      }
      break;

#ifdef RTLS_LOCATIONING_AOA
      case RTLS_CMD_AOA_SET_PARAMS:
      {
        RTLSCtrl_setAoaParamsCmd(pHostMsg->pData);

        RTLSUTIL_FREE(pHostMsg->pData);
      }
      break;

      case RTLS_CMD_AOA_ENABLE:
      {
        RTLSCtrl_enableAoaCmd(pHostMsg->pData);

        RTLSUTIL_FREE(pHostMsg->pData);
      }
      break;
#endif
#ifdef RTLS_PASSIVE
      case RTLS_CMD_CONN_PARAMS:
      {
        // Note that passive receives a different data structure than the master
        RTLSCtrl_connReqCmd(pHostMsg->pData);
      }
      break;
#endif

      case RTLS_CMD_CONN_INFO:
      {
        RTLSCtrl_enableConnInfoCmd(pHostMsg->pData);

        RTLSUTIL_FREE(pHostMsg->pData);
      }
      break;

      case RTLS_CMD_SET_RTLS_PARAM:
      {
        rtlsStatus_e status = RTLS_SUCCESS;

        uint8_t rtlsParamType = pHostMsg->pData[0];
        switch (rtlsParamType)
        {
          case RTLS_PARAM_CONNECTION_INTERVAL:
          {
            status = RTLSCtrl_updateConnInterval(&pHostMsg->pData[2]);
          }
          break;
          
          default:
          {
            status = RTLS_ILLEGAL_CMD;
          }
          break;
        }
        
        // return response with type and status
        setRtlsParamResponse_t response = {rtlsParamType, status};
        RTLSHost_sendMsg(RTLS_CMD_SET_RTLS_PARAM, HOST_SYNC_RSP, (uint8_t *)&response, sizeof(response));
      }
      break;

      default:
      {
        rtlsStatus_e status = RTLS_ILLEGAL_CMD;
        RTLSHost_sendMsg(RTLS_EVT_ERROR, HOST_ASYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));

        // If the opcode does not match anything, we need to free pData if it exists
        if (pHostMsg->pData)
        {
          RTLSUTIL_FREE(pHostMsg->pData);
        }
      }
      break;
    }
  }
}

/*********************************************************************
* @fn      RTLSCtrl_calculateRSSI
*
* @brief   This function will calculate the current RSSI based on RSSI
*          history and the current measurement
*
* @param   Last measured RSSI
*
* @return  none
*/
void RTLSCtrl_calculateRSSI(int lastRssi)
{
  gRtlsData.rssiFilter.currentRssi =
        ((RTLS_CTRL_ALPHA_FILTER_MAX_VALUE - gRtlsData.rssiFilter.alphaValue) * (gRtlsData.rssiFilter.currentRssi) + gRtlsData.rssiFilter.alphaValue * lastRssi) >> 4;
}

/*********************************************************************
* @fn      RTLSCtrl_malloc
*
* @brief   This function will allocate memory, if we were unable to allocate
*          we will report to RTLS Host
*
* @param   Allocated pointer - has to be cast
*
* @return  none
*/
void* RTLSCtrl_malloc(uint32_t sz)
{
  void *pPointer;

  RTLSUTIL_MALLOC(pPointer, sz);

  if (pPointer == NULL)
  {
    AssertHandler(RTLS_CTRL_ASSERT_CAUSE_OUT_OF_MEMORY, 0);
    return NULL;
  }

  return pPointer;
}

/*********************************************************************
 * @fn      RTLSCtrl_hostMsgCB
 *
 * @design /ref 159098678
 *
 * @brief   Callback from host when a message is available
 *          RTLS Control will enqueue the message and handle it in RTLS Control context
 *
 * @param   pMsg - uNPI message
 *
 * @return  none
 */
void RTLSCtrl_hostMsgCB(rtlsHostMsg_t *pMsg)
{
  RTLSCtrl_enqueueMsg(HOST_MSG_EVENT, (uint8_t *)pMsg);
}

/*********************************************************************
 * @fn      RTLSCtrl_enqueueMsg
 *
 * @design /ref 159098678
 *
 * @brief   Callback from RTLS host when an Rx msg is available
 *          Enqueue the message to switch the context from SWI to application
 *
 * @param   pMsg - pointer to a message
 * @param   eventId - needed to send message to correct handler
 *
 * @return  none
 */
void RTLSCtrl_enqueueMsg(uint16_t eventId, uint8_t *pMsg)
{
  rtlsEvt_t *qMsg;
  uint8_t enqueueStatus;
  volatile uint32_t keyHwi;
  rtlsStatus_e status = RTLS_OUT_OF_MEMORY;

  // Here we allocate the RTLS Event itself
  if ((qMsg = (rtlsEvt_t *)RTLSCtrl_malloc(sizeof(rtlsEvt_t))) == NULL)
  {
    return;
  }

  qMsg->event = (rtlsEvtType_e)eventId;
  qMsg->pData = pMsg;

  // Here we use Util to put the RTLS event into the RTLS Control Task queue
  keyHwi = Hwi_disable();
  enqueueStatus = Util_enqueueMsg(rtlsCtrlMsgQueue, syncRtlsEvent, (uint8_t *)qMsg);
  Hwi_restore(keyHwi);


  // Util failed to enqueue, report to host
  if (enqueueStatus == FALSE)
  {
    RTLSHost_sendMsg(RTLS_EVT_ERROR, HOST_ASYNC_RSP, (uint8_t *)&status, sizeof(rtlsStatus_e));
  }
}

/*********************************************************************
 * @fn      RTLSCtrl_processMessage
 *
 * @design /ref 159098678
 *
 * @brief   Process an incoming message
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
void RTLSCtrl_processMessage(rtlsEvt_t *pMsg)
{
  switch (pMsg->event)
  {
    case HOST_MSG_EVENT:
    {
      RTLSCtrl_processHostMessage((rtlsHostMsg_t *)pMsg->pData);
    }
    break;

    case RTLS_RUN_EVENT:
    {
      RTLSCtrl_processSyncEvent((uint8_t *)pMsg->pData);
    }
    break;

    default:
      // Do nothing.
      break;
  }

  if (pMsg->pData)
  {
    RTLSUTIL_FREE(pMsg->pData);
  }
}

/*********************************************************************
 * @fn      RTLSCtrl_createTask
 *
 * @design /ref 159098678
 *
 * @brief   Task creation function for the RTLS Control
 *
 * @param   none
 *
 * @return  none
 */
void RTLSCtrl_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = rtlsTaskStack;
  taskParams.stackSize = RTLS_CTRL_TASK_STACK_SIZE;
  taskParams.priority = RTLS_CTRL_TASK_PRIORITY;

  Task_construct(&rtlsTask, RTLSCtrl_taskFxn, &taskParams, NULL);
}

/*********************************************************************
 * @fn      RTLSCtrl_taskFxn
 *
 * @design /ref 159098678
 *
 * @brief   Main RTLS Control application RTOS task loop,
 *          handles starting NPI task and receiving messages
 *
 * @param   a0 - Standard TI RTOS taskFxn arguments.
 * @param   a1 -
 *
 * @return  none
 */
void RTLSCtrl_taskFxn(UArg a0, UArg a1)
{
  // Create an RTOS event used to wake up this application to process events.
  syncRtlsEvent = Event_create(NULL, NULL);

  if (syncRtlsEvent == NULL)
  {
    AssertHandler(RTLS_CTRL_ASSERT_CAUSE_NULL_POINTER_EXCEPT, 0);
  }

  // Create an RTOS queue for messages
  rtlsCtrlMsgQueue = Util_constructQueue(&rtlsCtrlMsg);

  // Initialize internal rssi alpha filter
  gRtlsData.rssiFilter.alphaValue = RTLS_CTRL_ALPHA_FILTER_VALUE;
  gRtlsData.rssiFilter.currentRssi = RTLS_CTRL_FILTER_INITIAL_RSSI;

  // Check if soft reset was made (as a result of reset_device request from the host)
  if (SysCtrlResetSourceGet() == RSTSRC_SYSRESET)
  {
    // Send response to the host that soft reset was made
    RTLSHost_sendMsg(RTLS_CMD_RESET_DEVICE, HOST_ASYNC_RSP, NULL, NULL);
  }

  for(;;)
  {
    volatile uint32_t keyHwi;
    uint32_t events = Event_pend(syncRtlsEvent, Event_Id_NONE, RTLS_CTRL_ALL_EVENTS, BIOS_WAIT_FOREVER);

    // If RTOS queue is not empty, process npi message.
    while(!Queue_empty(rtlsCtrlMsgQueue))
    {
      keyHwi = Hwi_disable();
      rtlsEvt_t *pMsg = (rtlsEvt_t *)Util_dequeueMsg(rtlsCtrlMsgQueue);
      Hwi_restore(keyHwi);

      if (pMsg)
      {
        // Process message.
        RTLSCtrl_processMessage(pMsg);

        RTLSUTIL_FREE(pMsg);
      }
    }
  }
}

/*********************************************************************
 * @fn      RTLSCtrl_sendDebugEvt
 *
 * @brief   Send debug info
 *
 * @param   debug_string
 * @param   debug_value
 *
 * @return  none
 */
void RTLSCtrl_sendDebugEvt(uint8_t *debug_string, uint32_t debug_value)
{
  debugInfo_t debugInfo;

  memcpy(debugInfo.debug_string, debug_string, DEBUG_STRING_SIZE-1);
  debugInfo.debug_string[DEBUG_STRING_SIZE-1] = NULL;
  debugInfo.debug_value = debug_value;

  RTLSHost_sendMsg(RTLS_EVT_DEBUG, HOST_ASYNC_RSP, (uint8_t *)&debugInfo, sizeof(debugInfo_t));
}

/*********************************************************************
 * @fn      RTLSCtrl_initAntenna
 *
 * @brief   Initialize a single pin to act as an antenna in case BOOSTXL-AOA is present
 *          In the case where BOOSTXL-AOA is not present, the pin specified will be set to high
 *
 * @param   pin - the pin to set
 * @param   pinState - 0 or 1
 *
 * @return  none - unlike other pin functions, this function won't return a pin handle
 *                 the user should be aware that a single pin is initialized
 */
void RTLSCtrl_initAntenna(uint8_t pin, uint8_t pinState)
{
  GPIO_setOutputEnableDio(pin, RTLS_TRUE);

  if (pinState == 1)
  {
    GPIO_setDio(pin);
  }
  else
  {
    GPIO_clearDio(pin);
  }
}
