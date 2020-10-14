/**************************************************************************************************
  Filename:       rtls_passive.c

  Description:    This file contains the RTLS Passive application
                  sample application definitions and prototypes.

* Copyright (c) 2017, Texas Instruments Incorporated
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* *  Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* *  Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* *  Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/

#include "rtls_passive.h"

#include <string.h>
#include <stdlib.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Hwi.h>

#include "bcomdef.h"
#include "micro_ble_cm.h"

#include "board.h"

// DriverLib
#include <driverlib/aon_batmon.h>
#include "rtls/rtls_ctrl_api.h"
#include "uble.h"
#include "ugap.h"
#include "urfc.h"

#include "util.h"
#include "gap.h"

// RTLS
#include "rtls/rtls_ble.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// Stack Task configuration
#define UBT_TASK_PRIORITY                     3

#ifndef UBT_TASK_STACK_SIZE
#define UBT_TASK_STACK_SIZE                   800
#endif

// App Task configuration
#define UCA_TASK_PRIORITY                     2

#ifndef UCA_TASK_STACK_SIZE
#define UCA_TASK_STACK_SIZE                   800
#endif

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
// Event globally used to post local events and pend on local events.
Event_Handle syncAppEvent;

// Event globally used to post local events and pend on local events.
static Event_Handle syncStackEvent;

Task_Struct ubtTask;
uint8 ubtTaskStack[UBT_TASK_STACK_SIZE];

Task_Struct ucaTask;
uint8 ucaTaskStack[UCA_TASK_STACK_SIZE];

// Queue object used for app messages
static Queue_Struct stackMsg;
static Queue_Handle stackMsgQueue;

// Queue object used for app messages
Queue_Struct appMsg;
Queue_Handle appMsgQueue;

// Flag to indicate whether we are reporting to RTLS Control or not
uint8_t gRtlsSyncEnabled = RTLS_FALSE;

// CM Session for RTLS Passive
uint8_t gCmSessionId = CM_INVALID_SESSION_ID;

// CM is tracking
uint8_t gMonitorTracking = RTLS_FALSE;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void RTLSPassive_taskFxn(UArg a0, UArg a1);

//ubStack High Priority Task
static void ubStack_taskFxn(UArg a0, UArg a1);

static void uBLEStack_eventProxy(void);
static bStatus_t RTLSPassive_enqueueStackMsg(uint16 event, uint8 data);

// General message handling
static void RTLSPassive_processRtlsPassiveMsg(uint8_t *pMsg);

// Handling CM events
static void RTLSPassive_processCmMsg(uint8_t *pMsg);
static void RTLSPassive_monitorIndicationEvt(uint8_t *pData);
static void RTLSPassive_monitorCompleteEvt(uint8_t *pData);
static void RTLSPassive_monitorStateChangeEvt(ugapMonitorState_t newState);

// Handling RTLS Control events
static void RTLSPassive_processRtlsCtrlMsg(uint8_t *pMsg);
static void RTLSPassive_cmStartReq(uint8_t *pConnInfo);
static void RTLSPassive_enableRtlsSync(uint8_t enable);
static void RTLSPassive_terminateLinkReq(void);

/*********************************************************************
 * @fn      RTLSPassive_processRtlsPassiveMsg
 *
 * @brief   General handling of application events
 *
 * @param   pMsg - a pointer to the message
 *
 * @return  none
 */
static void RTLSPassive_processRtlsPassiveMsg(uint8_t *pMsg)
{
  rtlsPassiveEvt_t *pEvt = (rtlsPassiveEvt_t *)pMsg;
  volatile uint32_t keyHwi;
  
  switch(pEvt->event)
  {
    case RTLS_PASSIVE_RTLS_CTRL_EVT:
    {
      RTLSPassive_processRtlsCtrlMsg((uint8_t *)pEvt->pData);
    }
    break;

    case RTLS_PASSIVE_CM_EVT:
    {
      RTLSPassive_processCmMsg((uint8_t *)pEvt->pData);
    }
    break;

    default:
      break;
  }

  keyHwi = Hwi_disable();
  free(pEvt->pData);
  Hwi_restore(keyHwi);
}

/*********************************************************************
 * @fn      RTLSPassive_processRtlsCtrlMsg
 *
 * @brief   Handle processing messages from RTLS Control
 *
 * @param   pMsg - a pointer to the message
 *
 * @return  none
 */
static void RTLSPassive_processRtlsCtrlMsg(uint8_t *pMsg)
{
  rtlsCtrlReq_t *pReq = (rtlsCtrlReq_t *)pMsg;
  volatile uint32_t keyHwi;

  switch(pReq->reqOp)
  {
    case RTLS_REQ_ENABLE_SYNC:
    {
      RTLSPassive_enableRtlsSync(*pReq->pData);
    }
    break;

    case RTLS_REQ_CONN:
    {
      RTLSPassive_cmStartReq(pReq->pData);
    }
    break;

    case RTLS_REQ_TERMINATE_LINK:
    {
      RTLSPassive_terminateLinkReq();
    }
    break;

    default:
      break;
  }

  keyHwi = Hwi_disable();
  free(pReq->pData);
  Hwi_restore(keyHwi);
}

/*********************************************************************
 * @fn      RTLSPassive_processCmMsg
 *
 * @brief   Handle processing messages from Connection Monitor
 *
 * @param   pMsg - a pointer to the message
 *
 * @return  none
 */
static void RTLSPassive_processCmMsg(uint8_t *pMsg)
{
  cmEvt_t *pEvt = (cmEvt_t *)pMsg;
  volatile uint32_t keyHwi;
  
  switch(pEvt->event)
  {
    case CM_MONITOR_STATE_CHANGED_EVT:
    {
      RTLSPassive_monitorStateChangeEvt(*pEvt->pData);
    }
    break;

    case CM_PACKET_RECEIVED_EVT:
    {
      RTLSPassive_monitorIndicationEvt(pEvt->pData);
    }
    break;

    case CM_CONN_EVT_COMPLETE_EVT:
    {
      RTLSPassive_monitorCompleteEvt(pEvt->pData);
    }
    break;

    default:
      break;
  }

  keyHwi = Hwi_disable();
  free(pEvt->pData);
  Hwi_restore(keyHwi);
}

/*********************************************************************
 * @fn      RTLSPassive_enableRtlsSync
 *
 * @brief   Enable RTLS synchronization
 *
 * @param   enable - enable/disable sync
 *
 * @return  none
 */
static void RTLSPassive_enableRtlsSync(uint8_t enable)
{
  if (enable == RTLS_TRUE)
  {
    gRtlsSyncEnabled = RTLS_TRUE;
  }
  else
  {
    gRtlsSyncEnabled = RTLS_FALSE;
  }
}

/*********************************************************************
 * @fn      RTLSPassive_cmStartReq
 *
 * @brief   Start Monitoring a BLE connection
 *
 * @param   pConnInfo - BLE connection parameters
 *
 * @return  none
 */
static void RTLSPassive_cmStartReq(uint8_t *pConnInfo)
{
  bleConnInfo_t *pBleConnInfo = (bleConnInfo_t *)pConnInfo;

  // When session already exists, just update by ubCM_updateExt (dont call ubCM_startExt)
  if (gCmSessionId != CM_INVALID_SESSION_ID)
  {
    ubCM_updateExt(gCmSessionId, pBleConnInfo->accessAddr,
                              pBleConnInfo->connInterval,
                              pBleConnInfo->hopValue,
                              pBleConnInfo->currChan,
                              pBleConnInfo->chanMap,
                              pBleConnInfo->crcInit);
    return;
  }

  // Mark that monitor is not currently tracking
  gMonitorTracking = RTLS_FALSE;

  // Kick CM
  gCmSessionId = ubCM_startExt(pBleConnInfo->accessAddr,
                               pBleConnInfo->connInterval,
                               pBleConnInfo->hopValue,
                               pBleConnInfo->currChan,
                               pBleConnInfo->chanMap,
                               pBleConnInfo->crcInit);
}

/*********************************************************************
 * @fn      RTLSPassive_terminateLinkReq
 *
 * @brief   Stop Monitoring a BLE connection
 *
 * @return  none
 */
static void RTLSPassive_terminateLinkReq(void)
{
  ubCM_stop(gCmSessionId);
  ugap_monitorStop();

  gCmSessionId = CM_INVALID_SESSION_ID;
  gRtlsSyncEnabled = RTLS_FALSE;
  gMonitorTracking = RTLS_FALSE;

  // Link terminated
  RTLSCtrl_connResultEvt(RTLS_LINK_TERMINATED);
}

/*********************************************************************
 * @fn      RTLSPassive_monitorIndicationEvt
 *
 * @brief   This function will be called for each BLE packet received
 *
 * @param   pData - Packet payload
 *
 * @return  none
 */
static void RTLSPassive_monitorIndicationEvt(uint8_t *pData)
{
}

/*********************************************************************
 * @fn      RTLSPassive_monitorCompleteEvt
 *
 * @brief   A single connection event has ended
 *
 * @param   pData - Session Id and Status of the session
 *
 * @return  none
 */
static void RTLSPassive_monitorCompleteEvt(uint8_t *pData)
{
  monitorCompleteEvt_t *pCompleteEvt = (monitorCompleteEvt_t *)pData;
  uint32_t nextStartTime;
  uint32_t currentTime;
  uint32_t nextEventTimeUs;
  int8_t slaveRssi;
  uint8_t channel;
  rtlsStatus_e status;
  port_key_t key;
  port_key_t key_h;

  // Convert CM Status to RTLS Status
  if (pCompleteEvt->status == CM_SUCCESS)
  {
    if (gMonitorTracking == RTLS_FALSE)
    {
      RTLSCtrl_connResultEvt(RTLS_SUCCESS);

      gMonitorTracking = RTLS_TRUE;
    }

    status = RTLS_SUCCESS;
  }
  else
  {
    status = RTLS_FAIL;
    RTLSPassive_terminateLinkReq();
  }
  

  // The timing of the next event is critical
  // Enter CS
  key_h = port_enterCS_HW();
  key = port_enterCS_SW();

  if (gRtlsSyncEnabled == RTLS_TRUE)
  {
    // Get the next start time
    nextStartTime = ubCMConnInfo.ArrayOfConnInfo[pCompleteEvt->sessionId - 1].nextStartTime;

    currentTime = RF_getCurrentTime();
    nextEventTimeUs = RF_convertRatTicksToUs(nextStartTime - currentTime);

    // We are interested in the slave RSSI and channel
    slaveRssi = (int8_t)ubCMConnInfo.ArrayOfConnInfo[pCompleteEvt->sessionId - 1].rssiSlave;
    channel = (uint8_t)ubCMConnInfo.ArrayOfConnInfo[pCompleteEvt->sessionId - 1].currentChan;

    RTLSCtrl_syncNotifyEvt(status, nextEventTimeUs, slaveRssi, channel);
  }

  // Exit CS
  port_exitCS_SW(key);
  port_exitCS_HW(key_h);
}

/*********************************************************************
 * @fn      RTLSPassive_monitorStateChangeEvt
 *
 * @brief   This function will be called for each BLE CM state change
 *
 * @param   newState - The new state
 *
 * @return  none
 */
static void RTLSPassive_monitorStateChangeEvt(ugapMonitorState_t newState)
{
  switch (newState)
  {
    case UGAP_MONITOR_STATE_INITIALIZED:
    {

    }
    break;

    case UGAP_MONITOR_STATE_IDLE:
    {

    }
    break;

    case UGAP_MONITOR_STATE_MONITORING:
    {

    }
    break;

    default:
      break;
  }
}


/*********************************************************************
 *  @fn      RTLSPassive_init
 *
 *  @brief   Called during initialization and contains application
 *           specific initialization (ie. hardware initialization/setup,
 *           table initialization, power up notification, etc), and
 *           profile initialization/setup.
 *
 *  @param   None
 *
 *  @return  None
 */
void RTLSPassive_init(void)
{
  Task_Params ucaTaskParams;

  // Create an RTOS event used to wake up this application to process events.
  syncAppEvent = Event_create(NULL, NULL);

  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);

  // Configure App task
  Task_Params_init(&ucaTaskParams);
  ucaTaskParams.stack = ucaTaskStack;
  ucaTaskParams.stackSize = UCA_TASK_STACK_SIZE;
  ucaTaskParams.priority = UCA_TASK_PRIORITY;
  Task_construct(&ucaTask, RTLSPassive_taskFxn, &ucaTaskParams, NULL);
}

/*********************************************************************
 * @fn      RTLSPassive_taskFxn
 *
 * @brief   Main CM application RTOS task loop, handles new CM command requests
 *          and continous monitor calls.
 *
 * @param   a0 - Standard TI RTOS taskFxn arguments.
 * @param   a1 -
 *
 * @return  none
 */
static void RTLSPassive_taskFxn(UArg a0, UArg a1)
{
  for(;;)
  {
    volatile uint32 keyHwi;
    uint32_t events = Event_pend(syncAppEvent, Event_Id_NONE, UCA_ALL_EVENTS, BIOS_WAIT_FOREVER);

    // If RTOS queue is not empty, process app message.
    while (!Queue_empty(appMsgQueue))
    {
      keyHwi = Hwi_disable();
      rtlsPassiveEvt_t *pMsg = (rtlsPassiveEvt_t *)Util_dequeueMsg(appMsgQueue);
      Hwi_restore(keyHwi);

      if (pMsg)
      {
        // Process message.
        RTLSPassive_processRtlsPassiveMsg((uint8_t *)pMsg);

        keyHwi = Hwi_disable();
        free(pMsg);
        Hwi_restore(keyHwi);
      }
    }
  }
}

/*********************************************************************
 * @fn      RTLSPassive_rtlsCtrlMsgCb
 *
 * @brief   Callback given to RTLS Control
 *
 * @param  cmd - the command to be enqueued
 *
 * @return  none
 */
void RTLSPassive_rtlsCtrlMsgCb(uint8_t *pCmd)
{
  // Enqueue the message to switch context
  RTLSPassive_enqueueAppMsg(RTLS_PASSIVE_RTLS_CTRL_EVT, (uint8_t *)pCmd);
}

/*********************************************************************
 * @fn      RTLSPassive_cmCb
 *
 * @brief   Callback given to Connection Monitor
 *
 * @param   pCmd - the command to be enqueued
 *
 * @return  none
 */
void RTLSPassive_cmCb(uint8_t *pCmd)
{
  // Enqueue the message to switch context
  RTLSPassive_enqueueAppMsg(RTLS_PASSIVE_CM_EVT, (uint8_t *)pCmd);
}

/*********************************************************************
 * @fn      RTLSPassive_enqueueMsg
 *
 * @design /ref 159098678
 *
 * @brief   Enqueue a message to RTLS Passive task function
 *
 * @param   pMsg - pointer to a message
 * @param   eventId - needed to send message to correct handler
 *
 * @return  none
 */
void RTLSPassive_enqueueAppMsg(uint16_t eventId, uint8_t *pMsg)
{
  rtlsPassiveEvt_t *qMsg;
  volatile uint32 keyHwi;

  keyHwi = Hwi_disable();
  if((qMsg = (rtlsPassiveEvt_t *)malloc(sizeof(rtlsPassiveEvt_t))))
  {
    qMsg->event = eventId;
    qMsg->pData = pMsg;

    Util_enqueueMsg(appMsgQueue, syncAppEvent, (uint8_t *)qMsg);
  }
  else
  {
    // Free pMsg if we failed to enqueue
    if (pMsg)
    {
      free(pMsg);
    }
  }
  Hwi_restore(keyHwi);
}

/*********************************************************************
 Setup and Run Required High Priority RTOS Task for ubStack functionality
 *********************************************************************/

/*********************************************************************
 * @fn      RTLSPassive_stack_init
 *
 * @brief   Initialize ubtTask.
 *
 * @param   none
 *
 * @return  none
 */
void RTLSPassive_stack_init(void)
{
  Task_Params stackTaskParams;

  // Configure Stack task
  Task_Params_init(&stackTaskParams);
  stackTaskParams.stack = ubtTaskStack;
  stackTaskParams.stackSize = UBT_TASK_STACK_SIZE;
  stackTaskParams.priority = UBT_TASK_PRIORITY;
  Task_construct(&ubtTask, ubStack_taskFxn, &stackTaskParams, NULL);
}

/*********************************************************************
 * @fn      ubStack_taskFxn
 *
 * @brief   Stack task entry point for the micro_ble.
 *
 * @param   none
 *
 * @return  none
 */
static void ubStack_taskFxn(UArg a0, UArg a1)
{
  volatile uint32 keyHwi;

  // Create an RTOS event used to wake up this application to process events.
  syncStackEvent = Event_create(NULL, NULL);

  // Create an RTOS queue for message from profile to be sent to app.
  stackMsgQueue = Util_constructQueue(&stackMsg);

  uble_stackInit(UBLE_ADDRTYPE_PUBLIC, NULL, uBLEStack_eventProxy,
                 RF_TIME_CRITICAL);

  ubCm_init(RTLSPassive_cmCb);

  for (;;)
  {
    // Waits for an event to be posted associated with the calling thread.
    // Note that an event associated with a thread is posted when a
    // message is queued to the message receive queue of the thread
    Event_pend(syncStackEvent, Event_Id_NONE, UBT_QUEUE_EVT, BIOS_WAIT_FOREVER);

    // If RTOS queue is not empty, process app message.
    while (!Queue_empty(stackMsgQueue))
    {
      ubtEvt_t *pMsg;

      // malloc() is not thread safe. Must disable HWI.
      keyHwi = Hwi_disable();
      pMsg = (ubtEvt_t *) Util_dequeueMsg(stackMsgQueue);
      Hwi_restore(keyHwi);

      if (pMsg)
      {
        // Only expects UBT_EVT_MICROBLESTACK from ubStack.
        if (pMsg->event == RTLS_PASSIVE_USTACK_EVT)
        {
          uble_processMsg();
        }

        // free() is not thread safe. Must disable HWI.
        keyHwi = Hwi_disable();

        // Free the space from the message.
        free(pMsg);
        Hwi_restore(keyHwi);
      }
    }
  }
}

/*********************************************************************
 * @fn      uBLEStack_eventProxy
 *
 * @brief   Required event_post for ubStack operation.
 *
 */
void uBLEStack_eventProxy(void)
{
  if (RTLSPassive_enqueueStackMsg(RTLS_PASSIVE_USTACK_EVT, 0) == FALSE)
  {
    // post event anyway when heap is out to avoid malloc error
    Event_post(syncStackEvent, UTIL_QUEUE_EVENT_ID);
  }
}

/*********************************************************************
 * @fn      RTLSPassive_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   data - message data.
 *
 * @return  TRUE or FALSE
 */
static bStatus_t RTLSPassive_enqueueStackMsg(uint16 event, uint8 data)
{
  volatile uint32 keyHwi;
  ubtEvt_t *pMsg;
  uint8_t status = FALSE;

  // malloc() is not thread safe. Must disable HWI.
  keyHwi = Hwi_disable();

  // Create dynamic pointer to message.
  pMsg = (ubtEvt_t*) malloc(sizeof(ubtEvt_t));
  if (pMsg != NULL)
  {
    pMsg->event = event;
    pMsg->data = data;

    // Enqueue the message.
    status = Util_enqueueMsg(stackMsgQueue, syncStackEvent, (uint8*) pMsg);
  }
  Hwi_restore(keyHwi);
  return status;
}

/*********************************************************************
 *********************************************************************/
