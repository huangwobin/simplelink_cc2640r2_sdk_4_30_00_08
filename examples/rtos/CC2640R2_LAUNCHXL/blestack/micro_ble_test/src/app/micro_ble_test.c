/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <stdlib.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>

#include "bcomdef.h"

#include "board.h"
#include "board_key.h"
#include <ti/display/Display.h>
#include <ti/display/lcd/LCDDogm1286.h>

// DriverLib
#include <driverlib/aon_batmon.h>
#include "uble.h"
#include "ugap.h"
#include "urfc.h"

#include "util.h"
#include "gap.h"

#ifndef POWER_TEST
#include <menu/two_btn_menu.h>
#include "micro_ble_test_menu.h"
#endif // !POWER_TEST
#include "micro_ble_test.h"

/*********************************************************************
 * MACROS
 */

// Eddystone Base 128-bit UUID: EE0CXXXX-8786-40BA-AB96-99B91AC981D8
#define EDDYSTONE_BASE_UUID_128( uuid )  0xD8, 0x81, 0xC9, 0x1A, 0xB9, 0x99, \
                                         0x96, 0xAB, 0xBA, 0x40, 0x86, 0x87, \
                           LO_UINT16( uuid ), HI_UINT16( uuid ), 0x0C, 0xEE

/*********************************************************************
 * CONSTANTS
 */

// Advertising interval (units of 0.625 millisec)
#ifdef POWER_TEST
#define DEFAULT_ADVERTISING_INTERVAL          160 // 100 ms
#endif // POWER_TEST

// Type of Display to open
#if !defined(Display_DISABLE_ALL)
  #if defined(BOARD_DISPLAY_USE_LCD) && (BOARD_DISPLAY_USE_LCD!=0)
    #define UBT_DISPLAY_TYPE Display_Type_LCD
  #elif defined (BOARD_DISPLAY_USE_UART) && (BOARD_DISPLAY_USE_UART!=0)
    #define UBT_DISPLAY_TYPE Display_Type_UART
  #else // !BOARD_DISPLAY_USE_LCD && !BOARD_DISPLAY_USE_UART
    #define UBT_DISPLAY_TYPE 0 // Option not supported
  #endif // BOARD_DISPLAY_USE_LCD && BOARD_DISPLAY_USE_UART
#else // BOARD_DISPLAY_USE_LCD && BOARD_DISPLAY_USE_UART
  #define UBT_DISPLAY_TYPE 0 // No Display
#endif // Display_DISABLE_ALL

// Task configuration
#define UBT_TASK_PRIORITY                     3

#ifndef UBT_TASK_STACK_SIZE
#define UBT_TASK_STACK_SIZE                   800
#endif

// RTOS Event to queue application events
#define UEB_QUEUE_EVT                         UTIL_QUEUE_EVENT_ID // Event_Id_30

// Application Events
#define UBT_EVT_KEY_CHANGE      0x0001
#define UBT_EVT_MICROBLESTACK   0x0002

// Pre-generated Random Static Address
#define UBT_PREGEN_RAND_STATIC_ADDR    {0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc}

// Eddystone definitions
#define EDDYSTONE_SERVICE_UUID                  0xFEAA

#define EDDYSTONE_FRAME_TYPE_UID                0x00
#define EDDYSTONE_FRAME_TYPE_URL                0x10
#define EDDYSTONE_FRAME_TYPE_TLM                0x20

#define EDDYSTONE_FRAME_OVERHEAD_LEN            8
#define EDDYSTONE_SVC_DATA_OVERHEAD_LEN         3
#define EDDYSTONE_MAX_URL_LEN                   18

// # of URL Scheme Prefix types
#define EDDYSTONE_URL_PREFIX_MAX        4
// # of encodable URL words
#define EDDYSTONE_URL_ENCODING_MAX      14

#define EDDYSTONE_URI_DATA_DEFAULT      "http://www.ti.com/ble"

// Row numbers
#define UBT_ROW_RESULT        TBM_ROW_APP
#define UBT_ROW_STATUS_1      (TBM_ROW_APP + 1)
#define UBT_ROW_STATUS_2      (TBM_ROW_APP + 2)
#define UBT_ROW_BCAST_STATE   (TBM_ROW_APP + 3)
#define UBT_ROW_SCAN_STATE    (TBM_ROW_APP + 4)
#define UBT_ROW_SCAN_1        (TBM_ROW_APP + 5)
#define UBT_ROW_SCAN_2        (TBM_ROW_APP + 6)
#define UBT_ROW_SCAN_3        (TBM_ROW_APP + 7)
#define UBT_ROW_MONITOR_STATE UBT_ROW_SCAN_STATE
#define UBT_ROW_MONITOR_1     UBT_ROW_SCAN_1
#define UBT_ROW_MONITOR_2     UBT_ROW_SCAN_2
#define UBT_ROW_MONITOR_3     UBT_ROW_SCAN_3

/*********************************************************************
 * TYPEDEFS
 */

// App to App event
typedef struct {
  uint16 event;
  uint8 data;
} ubtEvt_t;

// Eddystone UID frame
typedef struct {
  uint8 frameType;      // UID
  int8 rangingData;
  uint8 namespaceID[10];
  uint8 instanceID[6];
  uint8 reserved[2];
} eddystoneUID_t;

// Eddystone URL frame
typedef struct {
  uint8 frameType;      // URL | Flags
  int8 txPower;
  uint8 encodedURL[EDDYSTONE_MAX_URL_LEN];  // the 1st byte is prefix
} eddystoneURL_t;

// Eddystone TLM frame
typedef struct {
  uint8 frameType;      // TLM
  uint8 version;        // 0x00 for now
  uint8 vBatt[2];       // Battery Voltage, 1mV/bit, Big Endian
  uint8 temp[2];        // Temperature. Signed 8.8 fixed point
  uint8 advCnt[4];      // Adv count since power-up/reboot
  uint8 secCnt[4];      // Time since power-up/reboot
                          // in 0.1 second resolution
} eddystoneTLM_t;

typedef union {
  eddystoneUID_t uid;
  eddystoneURL_t url;
  eddystoneTLM_t tlm;
} eddystoneFrame_t;

typedef struct {
  uint8 length1;        // 2
  uint8 dataType1;      // for Flags data type (0x01)
  uint8 data1;          // for Flags data (0x04)
  uint8 length2;        // 3
  uint8 dataType2;      // for 16-bit Svc UUID list data type (0x03)
  uint8 data2;          // for Eddystone UUID LSB (0xAA)
  uint8 data3;          // for Eddystone UUID MSB (0xFE)
  uint8 length;         // Eddystone service data length
  uint8 dataType3;      // for Svc Data data type (0x16)
  uint8 data4;          // for Eddystone UUID LSB (0xAA)
  uint8 data5;          // for Eddystone UUID MSB (0xFE)
  eddystoneFrame_t frame;
} eddystoneAdvData_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */
// Display Interface
Display_Handle dispHandle = NULL;

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
static Event_Handle syncEvent;

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// Task configuration
Task_Struct ubtTask;
uint8 ubtTaskStack[UBT_TASK_STACK_SIZE];

#if defined(FEATURE_OBSERVER)
static bool ubTest_initObserver(void);
#endif /* FEATURE_OBSERVER */

#if defined(FEATURE_MONITOR) && !defined(FEATURE_CM)
static bool ubTest_initMonitor(void);
#endif /* FEATURE_MONITOR && !FEATURE_CM */

#if defined(FEATURE_CM)
extern bool ubCm_init(void);
#endif /* FEATURE_CM */

// Current Adv Interval
#ifdef POWER_TEST
static uint16 ubtAdvInterval = DEFAULT_ADVERTISING_INTERVAL;
#else // !POWER_TEST
static uint16 ubtAdvInterval = UBLE_PARAM_DFLT_ADVINTERVAL;
#endif // POWER_TEST

#if !defined(POWER_TEST) || defined(TX_POWER_SWEEP)
static uint32 advCount = 0;
#if defined(FEATURE_OBSERVER)
static uint32 scanIndCount = 0;
#endif /* FEATURE_OBSERVER */
#if defined(FEATURE_MONITOR)
static uint32 monitorCompleteCount = 0;
#endif /* FEATURE_NONITOR */
#endif // !POWER_TEST || TX_POWER_SWEEP

#if !defined(POWER_TEST)
// Current TX Power
static int8 ubtTxPower = UBLE_PARAM_DFLT_TXPOWER;
#elif defined(TX_POWER_SWEEP)
// Current TX Power
static int8 ubtTxPower = TX_POWER_MINUS_21_DBM;
#endif /* POWER_TEST, TX_POWER_SWEEP */

#ifndef POWER_TEST
// Pregenerated Random Static Addresses
uint8 staticAddr[][B_ADDR_LEN] = {
  /* 0xC0FFEEC0FFEE - Litten Endian */
  {0xEE, 0xFF, 0xC0, 0xEE, 0xFF, 0xC0},
  /* 0xDABBDABBDABB - Litten Endian */
  {0xBB, 0xDA, 0xBB, 0xDA, 0xBB, 0xDA},
  /* 0xFEEDBEEFF00D - Litten Endian */
  {0x0D, 0xF0, 0xEF, 0xBE, 0xED, 0xFE}
};

// Current Adv Channel Map
static uint8 ubtAdvChanMap = UBLE_PARAM_DFLT_ADVCHANMAP;

// Current Time to Adv
static uint8 ubtTimeToAdv = UBLE_PARAM_DFLT_TIMETOADV;

// Adv Data Update Option
static bool bUpdateUponEvent = false;

// Current Duty Off Time
static uint16 ubtBcastDutyOff = 0;
// Current Duty On Time
static uint16 ubtBcastDutyOn = 0;

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
static eddystoneAdvData_t eddystoneAdv = {
    // Flags; this sets the device to use general discoverable mode
    0x02,// length of this data
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED | GAP_ADTYPE_FLAGS_GENERAL,

    // Complete list of 16-bit Service UUIDs
    0x03,// length of this data including the data type byte
    GAP_ADTYPE_16BIT_COMPLETE, LO_UINT16(EDDYSTONE_SERVICE_UUID), HI_UINT16(
        EDDYSTONE_SERVICE_UUID),

    // Service Data
    0x03,// to be set properly later
    GAP_ADTYPE_SERVICE_DATA, LO_UINT16(EDDYSTONE_SERVICE_UUID), HI_UINT16(
        EDDYSTONE_SERVICE_UUID)
};

eddystoneUID_t eddystoneUID;
eddystoneURL_t eddystoneURL;
eddystoneTLM_t eddystoneTLM;

uint8 eddystoneURLDataLen;

// Array of URL Scheme Prefices
static char* eddystoneURLPrefix[EDDYSTONE_URL_PREFIX_MAX] = { "http://www.",
    "https://www.", "http://", "https://" };

// Array of URLs to be encoded
static char* eddystoneURLEncoding[EDDYSTONE_URL_ENCODING_MAX] = { ".com/",
    ".org/", ".edu/", ".net/", ".info/", ".biz/", ".gov/", ".com/", ".org/",
    ".edu/", ".net/", ".info/", ".biz/", ".gov/" };

// Eddystone frame type currently used
static uint8 currentFrameType = EDDYSTONE_FRAME_TYPE_UID;

// Pointer to application callback
keysPressedCB_t appKeyChangeHandler_st = NULL;
#else // POWER_TEST
/* This payload is from simple_broadcaster */
static uint8 advertData[] =
{
  // Flags; this sets the device to use limited discoverable
  // mode (advertises for 30 seconds at a time) instead of general
  // discoverable mode (advertises indefinitely)
  0x02,   // length of this data
  GAP_ADTYPE_FLAGS,
  GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

  // 25 byte beacon advertisement data
  // Preamble: Company ID - 0x000D for TI, refer to https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
  // Data type: Beacon (0x02)
  // Data length: 0x15
  // UUID: 00000000-0000-0000-0000-000000000000 (null beacon)
  // Major: 1 (0x0001)
  // Minor: 1 (0x0001)
  // Measured Power: -59 (0xc5)
  0x1A, // length of this data including the data type byte
  GAP_ADTYPE_MANUFACTURER_SPECIFIC, // manufacturer specific adv data type
  0x0D, // Company ID - Fixed
  0x00, // Company ID - Fixed
  0x02, // Data Type - Fixed
  0x15, // Data Length - Fixed
  0x00, // UUID - Variable based on different use cases/applications
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // UUID
  0x00, // Major
  0x01, // Major
  0x00, // Minor
  0x01, // Minor
  0xc5  // Power - The 2's complement of the calibrated Tx Power
};
#endif // POWER_TEST


/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void ubTest_init(void);
static void ubTest_taskFxn(UArg a0, UArg a1);

#ifndef POWER_TEST
static void ubTest_initUID(void);
static void ubTest_initURL(void);
static void ubTest_updateAdvDataWithFrame(uint8 frameType);

static void ubTest_keyChangeHandler(uint8 keys);
#endif // !POWER_TEST
static void ubTest_processAppMsg(ubtEvt_t *pMsg);

static void ubTest_bcast_stateChangeCB(ugapBcastState_t newState);
static void ubTest_bcast_advPrepareCB(void);
static void ubTest_bcast_advDoneCB(bStatus_t status);

#if defined(FEATURE_OBSERVER)
static void ubTest_scan_stateChangeCB(ugapObserverScan_State_t newState);
static void ubTest_scan_indicationCB(bStatus_t status, uint8_t len, uint8_t *pPayload);
static void ubTest_scan_windowCompleteCB(bStatus_t status);
#endif /* FEATURE_OBSERVER */

#if defined(FEATURE_MONITOR)
void ubTest_monitor_stateChangeCB(ugapMonitorState_t newState);
void ubTest_monitor_indicationCB(bStatus_t status, uint8_t sessionId, uint8_t len, uint8_t *pPayload);
void ubTest_monitor_completeCB(bStatus_t status, uint8_t sessionId);
#endif /* FEATURE_MONITOR */

static bStatus_t ubTest_enqueueMsg(uint16 event, uint8 data);

static void uBLEStack_eventProxy(void);

/*********************************************************************
 * CALLBACKS
 */

/*********************************************************************
 * @fn      ubTest_menuSwitchCB
 *
 * @brief   Callback function to be called when the menu is switching
 *
 * @param   pCurrMenu - the menu object the focus is leaving
 * @param   pNextMenu - the menu object the focus is moving into
 *
 * @return  None.
 */
void ubTest_menuSwitchCB(tbmMenuObj_t pCurrMenu, tbmMenuObj_t pNextMenu)
{
  return;
}


/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      ubTest_createTask
 *
 * @brief   Task creation function for the Micro Eddystone Beacon.
 *
 * @param   None.
 *
 * @return  None.
 */
void ubTest_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = ubtTaskStack;
  taskParams.stackSize = UBT_TASK_STACK_SIZE;
  taskParams.priority = UBT_TASK_PRIORITY;

  Task_construct(&ubtTask, ubTest_taskFxn, &taskParams, NULL);
}

/*********************************************************************
 * @fn      ubTest_init
 *
 * @brief   Initialization function for the Micro Eddystone Beacon App
 *          Task. This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification ...).
 *
 * @param   none
 *
 * @return  none
 */
static void ubTest_init(void)
{
#ifdef POWER_TEST
  ugapBcastCBs_t bcastCBs = {
  ubTest_bcast_stateChangeCB,
  ubTest_bcast_advPrepareCB,
  ubTest_bcast_advDoneCB };
#endif // POWER_TEST

  // Create an RTOS event used to wake up this application to process events.
  syncEvent = Event_create(NULL, NULL);

  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);

  // Default is not to switch antenna
  uble_registerAntSwitchCB(NULL);

#ifdef POWER_TEST
  uble_stackInit(UBLE_ADDRTYPE_PUBLIC, NULL, uBLEStack_eventProxy,
                 RF_TIME_CRITICAL);
  ugap_bcastInit(&bcastCBs);
#else // !POWER_TEST

#if !defined(NPI_USE_UART)
  // Setup keycallback for keys
  Board_initKeys(ubTest_keyChangeHandler);
#endif

  // Open Display.
  dispHandle = Display_open(UBT_DISPLAY_TYPE, NULL);

  // Initialize UID frame
  ubTest_initUID();

  // Initialize URL frame
  ubTest_initURL();

  /* Diable all menus except ubtMenuInit */
  tbm_setItemStatus(&ubtMenuMain, TBM_ITEM_0,
                    TBM_ITEM_1 | TBM_ITEM_2 | TBM_ITEM_3 | TBM_ITEM_4 |
                    TBM_ITEM_5 | TBM_ITEM_6 | TBM_ITEM_7);

  /* Disable stop action */
  tbm_setItemStatus(&ubtMenuBcastControl, TBM_ITEM_NONE, TBM_ITEM_2);

  #if defined(CC2650DK_7ID)
  tbm_initTwoBtnMenu(dispHandle, &ubtMenuMain, 2, NULL);
  #elif defined(CC2650_LAUNCHXL) || defined(CC2640R2_LAUNCHXL)
  tbm_initTwoBtnMenu(dispHandle, &ubtMenuMain, 3, NULL);
  #endif  // CC2650DK_7ID, CC2650_LAUNCHXL, CC2640R2_LAUNCHXL
#endif
}

#ifndef POWER_TEST
/*********************************************************************
 * @fn      ubTest_initUID
 *
 * @brief   initialize UID frame
 *
 * @param   none
 *
 * @return  none
 */
static void ubTest_initUID(void)
{
  // Set Eddystone UID frame with meaningless numbers for example.
  // This need to be replaced with some algorithm-based formula
  // for production.
  eddystoneUID.namespaceID[0] = 0x00;
  eddystoneUID.namespaceID[1] = 0x01;
  eddystoneUID.namespaceID[2] = 0x02;
  eddystoneUID.namespaceID[3] = 0x03;
  eddystoneUID.namespaceID[4] = 0x04;
  eddystoneUID.namespaceID[5] = 0x05;
  eddystoneUID.namespaceID[6] = 0x06;
  eddystoneUID.namespaceID[7] = 0x07;
  eddystoneUID.namespaceID[8] = 0x08;
  eddystoneUID.namespaceID[9] = 0x09;

  eddystoneUID.instanceID[0] = 0x04;
  eddystoneUID.instanceID[1] = 0x51;
  eddystoneUID.instanceID[2] = 0x40;
  eddystoneUID.instanceID[3] = 0x00;
  eddystoneUID.instanceID[4] = 0xB0;
  eddystoneUID.instanceID[5] = 0x00;
}

/*********************************************************************
 * @fn      ubTest_encodeURL
 *
 * @brief   Encodes URL in accordance with Eddystone URL frame spec
 *
 * @param   urlOrg - Plain-string URL to be encoded
 *          urlEnc - Encoded URL. Should be URLCFGSVC_CHAR_URI_DATA_LEN-long.
 *
 * @return  0 if the prefix is invalid
 *          The length of the encoded URL including prefix otherwise
 */
uint8 ubTest_encodeURL(char* urlOrg, uint8* urlEnc)
{
  uint8 i, j;
  uint8 urlLen;
  uint8 tokenLen;

  urlLen = (uint8) strlen(urlOrg);

  // search for a matching prefix
  for (i = 0; i < EDDYSTONE_URL_PREFIX_MAX; i++)
  {
    tokenLen = strlen(eddystoneURLPrefix[i]);
    if (strncmp(eddystoneURLPrefix[i], urlOrg, tokenLen) == 0)
    {
      break;
    }
  }

  if (i == EDDYSTONE_URL_PREFIX_MAX)
  {
    return 0;       // wrong prefix
  }

  // use the matching prefix number
  urlEnc[0] = i;
  urlOrg += tokenLen;
  urlLen -= tokenLen;

  // search for a token to be encoded
  for (i = 0; i < urlLen; i++)
  {
    for (j = 0; j < EDDYSTONE_URL_ENCODING_MAX; j++)
    {
      tokenLen = strlen(eddystoneURLEncoding[j]);
      if (strncmp(eddystoneURLEncoding[j], urlOrg + i, tokenLen) == 0)
      {
        // matching part found
        break;
      }
    }

    if (j < EDDYSTONE_URL_ENCODING_MAX)
    {
      memcpy(&urlEnc[1], urlOrg, i);
      // use the encoded byte
      urlEnc[i + 1] = j;
      break;
    }
  }

  if (i < urlLen)
  {
    memcpy(&urlEnc[i + 2], urlOrg + i + tokenLen, urlLen - i - tokenLen);
    return urlLen - tokenLen + 2;
  }

  memcpy(&urlEnc[1], urlOrg, urlLen);
  return urlLen + 1;
}

/*********************************************************************
 * @fn      ubTest_initUID
 *
 * @brief   initialize URL frame
 *
 * @param   none
 *
 * @return  none
 */
static void ubTest_initURL(void)
{
  // Set Eddystone URL frame with the URL of TI BLE site.
  eddystoneURLDataLen = ubTest_encodeURL(
      EDDYSTONE_URI_DATA_DEFAULT, eddystoneURL.encodedURL);
}

/*********************************************************************
 * @fn      ubTest_updateTLM
 *
 * @brief   Update TLM elements
 *
 * @param   none
 *
 * @return  none
 */
static void ubTest_updateTLM(void)
{
  uint32 time100MiliSec;
  uint32 batt;

  // Battery voltage (bit 10:8 - integer, but 7:0 fraction)
  batt = AONBatMonBatteryVoltageGet();
  batt = (batt * 125) >> 5; // convert V to mV
  eddystoneTLM.vBatt[0] = HI_UINT16(batt);
  eddystoneTLM.vBatt[1] = LO_UINT16(batt);

  // Temperature - 19.5 (Celcius) for example
  eddystoneTLM.temp[0] = 19;
  eddystoneTLM.temp[1] = 256 / 2;

  // advertise packet cnt;
  eddystoneTLM.advCnt[0] = BREAK_UINT32(advCount, 3);
  eddystoneTLM.advCnt[1] = BREAK_UINT32(advCount, 2);
  eddystoneTLM.advCnt[2] = BREAK_UINT32(advCount, 1);
  eddystoneTLM.advCnt[3] = BREAK_UINT32(advCount, 0);

  // running time
  // the number of 100-ms periods that have passed since the beginning.
  // no consideration of roll over for now.
  time100MiliSec = Clock_getTicks() / (100000 / Clock_tickPeriod);
  eddystoneTLM.secCnt[0] = BREAK_UINT32(time100MiliSec, 3);
  eddystoneTLM.secCnt[1] = BREAK_UINT32(time100MiliSec, 2);
  eddystoneTLM.secCnt[2] = BREAK_UINT32(time100MiliSec, 1);
  eddystoneTLM.secCnt[3] = BREAK_UINT32(time100MiliSec, 0);
}

/*********************************************************************
 * @fn      ubTest_selectFrame
 *
 * @brief   Selecting the type of frame to be put in the service data
 *
 * @param   frameType - Eddystone frame type
 *
 * @return  none
 */
static void ubTest_updateAdvDataWithFrame(uint8 frameType)
{
  if (frameType == EDDYSTONE_FRAME_TYPE_UID
      || frameType == EDDYSTONE_FRAME_TYPE_URL
      || frameType == EDDYSTONE_FRAME_TYPE_TLM)
  {
    eddystoneFrame_t* pFrame;
    uint8 frameSize;

    eddystoneAdv.length = EDDYSTONE_SVC_DATA_OVERHEAD_LEN;
    // Fill with 0s first
    memset((uint8*) &eddystoneAdv.frame, 0x00, sizeof(eddystoneFrame_t));

    switch (frameType) {
    case EDDYSTONE_FRAME_TYPE_UID:
      eddystoneUID.frameType = EDDYSTONE_FRAME_TYPE_UID;
      frameSize = sizeof(eddystoneUID_t);
      pFrame = (eddystoneFrame_t *) &eddystoneUID;
      break;

    case EDDYSTONE_FRAME_TYPE_URL:
      eddystoneURL.frameType = EDDYSTONE_FRAME_TYPE_URL;
      frameSize = sizeof(eddystoneURL_t) - EDDYSTONE_MAX_URL_LEN
          + eddystoneURLDataLen;
      pFrame = (eddystoneFrame_t *) &eddystoneURL;
      break;

    case EDDYSTONE_FRAME_TYPE_TLM:
      eddystoneTLM.frameType = EDDYSTONE_FRAME_TYPE_TLM;
      frameSize = sizeof(eddystoneTLM_t);
      pFrame = (eddystoneFrame_t *) &eddystoneTLM;
      break;
    }

    memcpy((uint8 *) &eddystoneAdv.frame, (uint8 *) pFrame, frameSize);
    eddystoneAdv.length += frameSize;

    uble_setParameter(UBLE_PARAM_ADVDATA,
                      EDDYSTONE_FRAME_OVERHEAD_LEN + eddystoneAdv.length,
                      &eddystoneAdv);
  }
}

/*********************************************************************
 * @fn      ubTest_bcast_updateAdvData(void)
 *
 * @brief   Update Adv Data.
 *
 * @param   None.
 *
 * @return  None.
 */
static void ubTest_bcast_updateAdvData(void)
{
  if (currentFrameType == EDDYSTONE_FRAME_TYPE_TLM)
  {
    ubTest_updateTLM();
  }

  ubTest_updateAdvDataWithFrame(currentFrameType);
}
#endif // !POWER_TEST

/*********************************************************************
 * @fn      ubTest_processEvent
 *
 * @brief   Application task entry point for the Micro Eddystone Beacon.
 *
 * @param   none
 *
 * @return  none
 */
static void ubTest_taskFxn(UArg a0, UArg a1)
{
  volatile uint32 keyHwi;

  // Initialize application
  ubTest_init();

  for (;;)
  {
    // Waits for an event to be posted associated with the calling thread.
    // Note that an event associated with a thread is posted when a
    // message is queued to the message receive queue of the thread
    Event_pend(syncEvent, Event_Id_NONE, UEB_QUEUE_EVT, BIOS_WAIT_FOREVER);

    // If RTOS queue is not empty, process app message.
    while (!Queue_empty(appMsgQueue))
    {
      ubtEvt_t *pMsg;

      // malloc() is not thread safe. Must disable HWI.
      keyHwi = Hwi_disable();
      pMsg = (ubtEvt_t *) Util_dequeueMsg(appMsgQueue);
      Hwi_restore(keyHwi);

      if (pMsg)
      {
        // Process message.
        ubTest_processAppMsg(pMsg);

        // free() is not thread safe. Must disable HWI.
        keyHwi = Hwi_disable();

        // Free the space from the message.
        free(pMsg);
        Hwi_restore(keyHwi);
      }
    }
  }
}

#ifndef POWER_TEST
/*********************************************************************
 * @fn      ubTest_handleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 KEY_LEFT
 *                 KEY_RIGHT
 *
 * @return  none
 */
static void ubTest_handleKeys(uint8 shift, uint8 keys) {
  (void) shift;  // Intentionally unreferenced parameter

  if (keys & KEY_LEFT)
  {
    // Check if the key is still pressed. WA for the terrible debouncing.
#if defined(CC2650DK_7ID)
    if (PIN_getInputValue(Board_KEY_LEFT) == 0)
#elif defined(CC2650_LAUNCHXL) || defined(CC2640R2_LAUNCHXL)
    if (PIN_getInputValue(Board_PIN_BUTTON0) == 0)
#endif // CC2650DK_7ID, CC2650_LAUNCHXL, CC2640R2_LAUNCHXL
    {
      tbm_buttonLeft();
    }
  }
  else if (keys & KEY_RIGHT)
  {
    // Check if the key is still pressed. WA for the terrible debouncing.
#if defined(CC2650DK_7ID)
    if (PIN_getInputValue(Board_KEY_RIGHT) == 0)
#elif defined(CC2650_LAUNCHXL) || defined(CC2640R2_LAUNCHXL)
    if (PIN_getInputValue(Board_PIN_BUTTON1) == 0)
#endif // CC2650DK_7ID, CC2650_LAUNCHXL, CC2640R2_LAUNCHXL
    {
      tbm_buttonRight();
    }
  }
}

/*********************************************************************
 * @fn      ubTest_keyChangeHandler
 *
 * @brief   Key event handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
void ubTest_keyChangeHandler(uint8 keys)
{
  ubTest_enqueueMsg(UBT_EVT_KEY_CHANGE, keys);
}
#endif  // !POWER_TEST

/*********************************************************************
 * @fn      ubTest_processAppMsg
 *
 * @brief   Process an incoming callback from a profile.
 *
 * @param   pMsg - message to process
 *
 * @return  None.
 */
static void ubTest_processAppMsg(ubtEvt_t *pMsg)
{
  switch (pMsg->event)
  {
#ifndef POWER_TEST
  case UBT_EVT_KEY_CHANGE:
    ubTest_handleKeys(0, pMsg->data);
    break;
#endif // !POWER_TEST

  case UBT_EVT_MICROBLESTACK:
    uble_processMsg();
    break;

  default:
    // Do nothing.
    break;
  }
}

/*********************************************************************
 * @fn      ubTest_bcast_stateChange_CB
 *
 * @brief   Callback from Micro Broadcaster indicating a state change.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void ubTest_bcast_stateChangeCB(ugapBcastState_t newState)
{
  switch (newState)
  {
  case UGAP_BCAST_STATE_INITIALIZED:
#ifdef POWER_TEST
    {
#ifdef TX_POWER_SWEEP
      int8 txPower = ubtTxPower;
      uble_setParameter(UBLE_PARAM_TXPOWER, sizeof(uint8), &txPower);
#endif // TX_POWER_SWEEP
      uble_setParameter(UBLE_PARAM_ADVINTERVAL, sizeof(uint16), &ubtAdvInterval);
      uble_setParameter(UBLE_PARAM_ADVDATA, sizeof(advertData), advertData);
      ugap_bcastStart(0);
    }
#else // !POWER_TEST
    Display_print0(dispHandle, UBT_ROW_BCAST_STATE, 0,
                   "UBLE_State: Initialized");
#endif // POWER_TEST
    break;

  case UGAP_BCAST_STATE_IDLE:
    Display_print0(dispHandle, UBT_ROW_BCAST_STATE, 0,
                   "UBLE_State: Idle");
    break;

  case UGAP_BCAST_STATE_ADVERTISING:
    Display_print0(dispHandle, UBT_ROW_BCAST_STATE, 0,
                   "UBLE_State: Advertising");
    break;

  case UGAP_BCAST_STATE_WAITING:
    Display_print0(dispHandle, UBT_ROW_BCAST_STATE, 0,
                   "UBLE_State: Waiting");
    break;

  default:
    break;
  }
}

/*********************************************************************
 * @fn      ubTest_bcast_advPrepareCB
 *
 * @brief   Callback from Micro Broadcaster notifying that the next
 *          advertising event is about to start so it's time to update
 *          the adv payload.
 *
 * @param   None.
 *
 * @return  None.
 */
static void ubTest_bcast_advPrepareCB(void)
{
#ifndef POWER_TEST
  static uint8* pStr[3] = {".", "..", "..."};
  static uint8 progress = 0;

  ubTest_bcast_updateAdvData();

  Display_print1(dispHandle, UBT_ROW_RESULT, 0,
                 "Adv Prepare CB%s", pStr[progress]);

  if (++progress == 3)
  {
    progress = 0;
  }
#endif // POWER_TEST
}

/*********************************************************************
 * @fn      ubTest_bcast_advDoneCB
 *
 * @brief   Callback from Micro Broadcaster notifying that an
 *          advertising event has been done.
 *
 * @param   status - How the last event was done. SUCCESS or FAILURE.
 *
 * @return  None.
 */
static void ubTest_bcast_advDoneCB(bStatus_t status)
{
#ifndef POWER_TEST
  advCount++;

  Display_print1(dispHandle, UBT_ROW_STATUS_2, 0,
                 "%d Adv\'s done", advCount);
#endif // !POWER_TEST

#ifdef TX_POWER_SWEEP
  /* Increase TX power at every 7th adv done */
  if ((++advCount % 7) == 0)
  {
    if (ubtTxPower < TX_POWER_5_DBM)
    {
      uint8 i;

      /* Get the index of the current TX Power in the power table */
      for (i = 0; i < ubTxPowerTable.numTxPowerVal; i++)
      {
        if (ubTxPowerTable.pTxPowerVals[i].dBm == ubtTxPower)
        {
          break;
        }
      }

      ubtTxPower = ubTxPowerTable.pTxPowerVals[i + 1].dBm;
      uble_setParameter(UBLE_PARAM_TXPOWER, sizeof(ubtTxPower), &ubtTxPower);
    }
  }
#endif // TX_POWER_SWEEP
}


/*********************************************************************
 * @fn      ubTest_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   data - message data.
 *
 * @return  TRUE or FALSE
 */
static bStatus_t ubTest_enqueueMsg(uint16 event, uint8 data)
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
    status = Util_enqueueMsg(appMsgQueue, syncEvent, (uint8*) pMsg);
  }
  Hwi_restore(keyHwi);
  return status;
}

void uBLEStack_eventProxy(void)
{
  ubTest_enqueueMsg(UBT_EVT_MICROBLESTACK, 0);
}

#ifndef POWER_TEST
/* Common for ubTest_doInitAddrXXXXXXXX() functions */
bool ubTest_initBcast(void)
{
  ugapBcastCBs_t bcastCBs = {
    ubTest_bcast_stateChangeCB,
    ubTest_bcast_advPrepareCB,
    ubTest_bcast_advDoneCB };

  /* Initilaize Micro GAP Broadcaster Role */
  if (SUCCESS == ugap_bcastInit(&bcastCBs))
  {
    uint8 tempPower;

    Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Bcast initialized");

    uble_getParameter(UBLE_PARAM_TXPOWER, &tempPower);
    eddystoneUID.rangingData = tempPower;
    eddystoneURL.txPower = tempPower;

    currentFrameType = EDDYSTONE_FRAME_TYPE_UID;

    ubTest_updateAdvDataWithFrame(currentFrameType);

    return true;
  }

  Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Bcast init failed");

  return false;
}

#if defined(FEATURE_OBSERVER)
bool ubTest_initObserver(void)
{
  ugapObserverScanCBs_t observerCBs = {
    ubTest_scan_stateChangeCB,
    ubTest_scan_indicationCB,
    ubTest_scan_windowCompleteCB };

  /* Initilaize Micro GAP Observer Role */
  if (SUCCESS == ugap_scanInit(&observerCBs))
  {
    Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Observer initialized");

    /* This is the spot to do scan reuqest without using the keypress control */
    ugap_scanRequest(UBLE_ADV_CHAN_ALL, 160, 320);

    return true;
  }

  Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Observer init failed");

  return false;
}
#endif /* FEATURE_OBSERVER */

#if defined(FEATURE_MONITOR) && !defined(FEATURE_CM)
bool ubTest_initMonitor(void)
{
  ugapMonitorCBs_t monitorCBs = {
    ubTest_monitor_stateChangeCB,
    ubTest_monitor_indicationCB,
    ubTest_monitor_completeCB };

  /* Initilaize Micro GAP Monitor */
  if (SUCCESS == ugap_monitorInit(&monitorCBs))
  {
    Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Monitor initialized");
    return true;
  }

  Display_print0(dispHandle, UBT_ROW_STATUS_2, 0, "Monitor init failed");

  return false;
}
#endif /* FEATURE_OBSERVER && !FEATURE_CM */

/* Common for ubTest_doInitAddrXXXXXX() */
bool ubTest_initPostProcess(bStatus_t status, uint8* pAddrType)
{
  bool bSuccess = false;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_2);

  if (status == SUCCESS)
  {
    uint8 bdAddr[B_ADDR_LEN];

    uble_getAddr(UBLE_ADDRTYPE_BD, bdAddr);

    Display_print1(dispHandle, UBT_ROW_RESULT, 0, "Init\'ed w/ %s", pAddrType);
    Display_print1(dispHandle, UBT_ROW_STATUS_1, 0,
                   "BDAddr=%s", Util_convertBdAddr2Str(bdAddr));

    bSuccess = ubTest_initBcast();

    /* Enable all items but ubtMenuInit */
    tbm_setItemStatus(&ubtMenuMain,
                      TBM_ITEM_1 | TBM_ITEM_2 | TBM_ITEM_3 | TBM_ITEM_4 |
                      TBM_ITEM_5 | TBM_ITEM_6 | TBM_ITEM_7,
                      TBM_ITEM_0);

    /* Switch to ubtMenuMain */
    tbm_goTo(&ubtMenuMain);
  }
  else {
    Display_print0(dispHandle, UBT_ROW_RESULT, 0, "Stack init failed");
  }

#if defined(FEATURE_OBSERVER)
  /* This is the sopt to init Observer if not keypress controlled */
  bSuccess = ubTest_initObserver();
#endif /* FEATURE_OBSERVER */

#if defined(FEATURE_MONITOR)
#if defined(FEATURE_CM)
  bSuccess = ubCm_init();
#else
  /* This is the sopt to init Monitor if not keypress controlled */
  bSuccess = ubTest_initMonitor();
#endif /* FEATURE_CM */
#endif /* FEATURE_MONITOR */
  return bSuccess;
}

/* Actions for Menu: Init - Addr Public */
bool ubTest_doInitAddrPublic(uint8 index)
{
  bStatus_t status;

  (void) index;

  status = uble_stackInit(UBLE_ADDRTYPE_PUBLIC, NULL, uBLEStack_eventProxy,
                          RF_TIME_CRITICAL);

  return ubTest_initPostProcess(status, "PubAddr");
}

/* Action for Menu: Init - Addr Static - Select - XXXXXXXXXXXX */
bool ubTest_doInitAddrStaticSelect(uint8 index)
{
  bStatus_t status;

  /* Pregenerated addresses start at the item 1 position */
  index -= 1;

  status = uble_stackInit(UBLE_ADDRTYPE_STATIC, staticAddr[index],
                          uBLEStack_eventProxy, RF_TIME_CRITICAL);

  return ubTest_initPostProcess(status, "StatAddr");
}

/* Actions for Menu: Init - Addr Static - Generate */
bool ubTest_doInitAddrStaticGenerate(uint8 index)
{
  bStatus_t status;

  (void) index;

  status = uble_stackInit(UBLE_ADDRTYPE_STATIC, NULL, uBLEStack_eventProxy,
                          RF_TIME_CRITICAL);

  return ubTest_initPostProcess(status, "AddrGen");
}

/* Common to Increment/Decrement actions */
void ubTest_displayOutOfRange(uint8 inc_dec)
{
  Display_print1(dispHandle, UBT_ROW_RESULT, 0, "No more %sc allowed",
                 inc_dec ? "de" : "in");
}

/* Actions for Menu: TX Power */
bool ubTest_doTxPower(uint8 index)
{
  uint8 i;
  bool bSuccess = false;

  /* Get the index of the current TX Power in the power table */
  for (i = 0; i < ubTxPowerTable.numTxPowerVal; i++)
  {
    if (ubTxPowerTable.pTxPowerVals[i].dBm == ubtTxPower)
    {
      break;
    }
  }

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (i < ubTxPowerTable.numTxPowerVal - 1) /* If not maximum already? */
    {
      ubtTxPower = ubTxPowerTable.pTxPowerVals[i + 1].dBm;
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (i > 0) /* If not minimum already? */
    {
      ubtTxPower = ubTxPowerTable.pTxPowerVals[i - 1].dBm;
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == uble_setParameter(UBLE_PARAM_TXPOWER,
                                     sizeof(ubtTxPower), &ubtTxPower))
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0,
                     "TXPower: %d dBm", ubtTxPower);

      eddystoneUID.rangingData = ubtTxPower;
      eddystoneURL.txPower = ubtTxPower;
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "TXPower failed");
      /* Restore ubtTxPower */
      uble_getParameter(UBLE_PARAM_TXPOWER, &ubtTxPower);
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Actions for Menu: Adv Interval */
bool ubTest_doAdvInterval(uint8 index)
{
  bool bSuccess = false;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (ubtAdvInterval <= UBLE_MAX_ADV_INTERVAL - 160)
    {
      ubtAdvInterval += 160;  /* Add 100 ms */
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (ubtAdvInterval >= UBLE_MIN_ADV_INTERVAL + 160)
    {
      ubtAdvInterval -= 160;  /* Subtract 100 ms */
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == uble_setParameter(UBLE_PARAM_ADVINTERVAL,
                                     sizeof(ubtAdvInterval), &ubtAdvInterval))
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0,
                     "AdvInterval: %d", ubtAdvInterval);
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "AdvInterval Failed");
      /* Restore ubtAdvInterval */
      uble_getParameter(UBLE_PARAM_ADVINTERVAL, &ubtAdvInterval);
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Actions for Menu: Adv Channel Map */
bool ubTest_doAdvChanMap(uint8 index)
{
  bool bSuccess = false;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (ubtAdvChanMap < UBLE_MAX_CHANNEL_MAP)
    {
      ubtAdvChanMap++;
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (ubtAdvChanMap > UBLE_MIN_CHANNEL_MAP)
    {
      ubtAdvChanMap--;
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == uble_setParameter(UBLE_PARAM_ADVCHANMAP,
                                     sizeof(ubtAdvChanMap), &ubtAdvChanMap))
    {
      Display_print3(dispHandle, UBT_ROW_RESULT, 0, "AdvChan: %s %s %s",
                     (ubtAdvChanMap & UBLE_ADV_CHAN_37) ? "37" : "  ",
                     (ubtAdvChanMap & UBLE_ADV_CHAN_38) ? "38" : "  ",
                     (ubtAdvChanMap & UBLE_ADV_CHAN_39) ? "39" : "  ");
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "AdvChan failed");
      /* Restore ubtAdvChanMap */
      uble_getParameter(UBLE_PARAM_ADVCHANMAP, &ubtAdvChanMap);
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Actions for Menu: Time to Prepare */
bool ubTest_doTimeToPrepare(uint8 index)
{
  bool bSuccess = false;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (ubtTimeToAdv < 254)
    {
      ubtTimeToAdv += 2;
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (ubtTimeToAdv > 1)
    {
      ubtTimeToAdv -= 2;
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == uble_setParameter(UBLE_PARAM_TIMETOADV,
                                     sizeof(ubtTimeToAdv), &ubtTimeToAdv))
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0,
                     "TimeToPrepare: %d ms", ubtTimeToAdv);
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "TimeToPrepare failed");
      /* Restore ubtTimeToAdv */
      uble_getParameter(UBLE_PARAM_TIMETOADV, &ubtTimeToAdv);
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Actions for Menu: Adv Data */
bool ubTest_doSetFrameType(uint8 index)
{
  static uint8* pType[3] = { "UID", "URL", "TLM" };

  /* Items for this action starts at ROW 2 in ubtMenuAdvData */
  index--;

  Display_print2(dispHandle, UBT_ROW_RESULT, 0,
                 "%sEddystone %s!", (bUpdateUponEvent)? "To be " : "",
                 pType[index]);

  currentFrameType = index << 4;

  if (!bUpdateUponEvent)
  {
    ubTest_bcast_updateAdvData();
  }

  return true;
}

/* Actions for Menu: Update Option */
bool ubTest_doUpdateOption(uint8 index)
{
  Display_print0(dispHandle, UBT_ROW_RESULT, 0,
                 (index == 0) ? "Immediate Change" : "Change upon Event");

  bUpdateUponEvent = (index == 0) ? false : true;

  /* Go to ubtMenuAdvData object */
  tbm_goTo(&ubtMenuAdvData);

  return true;
}

/* Actions for Menu: Bcast Duty - On Time */
bool ubTest_doBcastDutyOnTime(uint8 index)
{
  bool bSuccess = false;
  uint16 currentDutyOn = ubtBcastDutyOn;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (ubtBcastDutyOn <= 65535 - 5)
    {
      ubtBcastDutyOn += 5;  /* Add 500 ms */
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (ubtBcastDutyOn >= 5)
    {
      ubtBcastDutyOn -= 5;  /* Subtract 500 ms */
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == ugap_bcastSetDuty(ubtBcastDutyOn, ubtBcastDutyOff))
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0, "DutyOnTime: %d ms",
                     ubtBcastDutyOn * 100);
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "DutyOnTime failed");
      /* Restore ubtAdvInterval */
      ubtBcastDutyOn = currentDutyOn;
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Actions for Menu: Bcast Duty - Off Time */
bool ubTest_doBcastDutyOffTime(uint8 index)
{
  bool bSuccess = false;
  uint16 currentDutyOff = ubtBcastDutyOff;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_1);

  if (index == 0) /* Increment */
  {
    if (ubtBcastDutyOff <= 65535 - 5)
    {
      ubtBcastDutyOff += 5;  /* Add 500 ms */
      bSuccess = true;
    }
  }
  else /* index == 1, Decrement */
  {
    if (ubtBcastDutyOff >= 5)
    {
      ubtBcastDutyOff -= 5;  /* Subtract 500 ms */
      bSuccess = true;
    }
  }

  if (bSuccess)
  {
    if (SUCCESS == ugap_bcastSetDuty(ubtBcastDutyOn, ubtBcastDutyOff))
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0,
                     "DutyOffTime: %d ms",
                     ubtBcastDutyOff * 100);

      if (ubtBcastDutyOff == 0)
      {
        Display_print0(dispHandle, UBT_ROW_STATUS_1, 0, "DutyControl disabled");
      }
    }
    else
    {
      bSuccess = false;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "DutyOffTime failed");
      /* Restore ubtAdvInterval */
      ubtBcastDutyOff = currentDutyOff;
    }
  }
  else
  {
    ubTest_displayOutOfRange(index);
  }

  return bSuccess;
}

/* Menu: Bcast Control */
bool ubTest_doBcastStart(uint8 index)
{
  bool bSuccess = false;
  uint16 numAdv = index ? 100 : 0;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_2);

  if (SUCCESS == ugap_bcastStart(numAdv))
  {
    if (numAdv == 0)
    {
      bSuccess = true;
      Display_print0(dispHandle, UBT_ROW_RESULT, 0, "Bcast - indefinitely");
    }
    else
    {
      Display_print1(dispHandle, UBT_ROW_RESULT, 0, "Bcast - %d Adv\'s", numAdv);
    }

    /* Enable stop action and disable start actions  */
    tbm_setItemStatus(&ubtMenuBcastControl, TBM_ITEM_2,
                      TBM_ITEM_0 | TBM_ITEM_1);
  }
  else
  {
    Display_print0(dispHandle, UBT_ROW_RESULT, 0, "Bcast start failed");
  }

  return bSuccess;
}

bool ubTest_doBcastStop(uint8 index)
{
  bool bSuccess = false;

  (void) index;

  Display_clearLines(dispHandle, UBT_ROW_STATUS_1, UBT_ROW_STATUS_2);

  if (SUCCESS == ugap_bcastStop())
  {
    bSuccess = true;
    Display_print0(dispHandle, UBT_ROW_RESULT, 0, "Bcast - stopped");

    /* Enable start actions */
    tbm_setItemStatus(&ubtMenuBcastControl,
                      TBM_ITEM_0 | TBM_ITEM_1, TBM_ITEM_2);
  }
  else
  {
    Display_print0(dispHandle, UBT_ROW_RESULT, 0, "Bcast stop failed");
  }

  return bSuccess;
}

#if defined(FEATURE_OBSERVER)

/* *******************************************************************
 *                     Obesrver test functions
 * *******************************************************************/

/*********************************************************************
 * @fn      ubTest_scan_stateChangeCB
 *
 * @brief   Callback from Micro Observer indicating a state change.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void ubTest_scan_stateChangeCB(ugapObserverScan_State_t newState)
{
  switch (newState)
  {
  case UGAP_SCAN_STATE_INITIALIZED:
    Display_print0(dispHandle, UBT_ROW_SCAN_STATE, 0,
                   "Observer_State: Initialized");
    break;

  case UGAP_SCAN_STATE_IDLE:
    Display_print0(dispHandle, UBT_ROW_SCAN_STATE, 0,
                   "Observer_State: Idle");
    break;

  case UGAP_SCAN_STATE_SCANNING:
    Display_print0(dispHandle, UBT_ROW_SCAN_STATE, 0,
                   "Observer_State: Scanning");
    break;

  case UGAP_SCAN_STATE_WAITING:
    Display_print0(dispHandle, UBT_ROW_SCAN_STATE, 0,
                   "Observer_State: Waiting");
    break;

  case UGAP_SCAN_STATE_SUSPENDED:
    Display_print0(dispHandle, UBT_ROW_SCAN_STATE, 0,
                   "Observer_State: Suspended");
    break;

  default:
    break;
  }
}

/*********************************************************************
 * @fn      ubTest_scan_indicationCB
 *
 * @brief   Callback from Micro observer notifying that a advertising
 *          packet is received.
 *
 * @param   status status of a scan
 * @param   len length of the payload
 * @param   pPayload pointer to payload
 *
 * @return  None.
 */
static void ubTest_scan_indicationCB(bStatus_t status, uint8_t len, uint8_t *pPayload)
{
  static char  *devAddr;
  static uint8  chan;
  static uint32 timeStamp;
  static uint32 rxBufFull = 0;

  /* We have an advertisment packe:
   *
   * | Preamble  | Access Addr | PDU         | CRC     |
   * | 1-2 bytes | 4 bytes     | 2-257 bytes | 3 bytes |
   *
   * The PDU is expended to:
   * | Header  | Payload     |
   * | 2 bytes | 1-255 bytes |
   *
   * The Header is expended to:
   * | PDU Type...RxAdd | Length |
   * | 1 byte           | 1 byte |
   *
   * The Payload is expended to:
   * | AdvA    | AdvData    |
   * | 6 bytes | 0-31 bytes |
   *
   * The radio stripps the CRC and replaces it with the postfix.
   *
   * The Postfix is expended to:
   * | RSSI   | Status | TimeStamp |
   * | 1 byte | 1 byte | 4 bytes   |
   *
   * The Status is expended to:
   * | bCrcErr | bIgnore | channel  |
   * | bit 7   | bit 6   | bit 5..0 |
   *
   * Note the advPke is the beginning of PDU; the dataLen includes
   * the postfix length.
   *
   */
  if (status == SUCCESS)
  {
    devAddr = Util_convertBdAddr2Str(pPayload + 2);
    chan = (*(pPayload + len - 5) & 0x3F);
    timeStamp = *(uint32 *)(pPayload + len - 4);

    Display_print3(dispHandle, UBT_ROW_SCAN_1, 0, "ScanInd DevAddr: %s Chan: %d timeStamp: %ul", devAddr, chan, timeStamp);
  }
  else
  {
    /* Rx buffer is full */
    Display_print1(dispHandle, UBT_ROW_SCAN_3, 0, "Rx Buffer Full: %d", ++rxBufFull);
  }
}

/*********************************************************************
 * @fn      ubTest_scan_windowCompleteCB
 *
 * @brief   Callback from Micro Broadcaster notifying that a
 *          scan window is completed.
 *
 * @param   status - How the last event was done. SUCCESS or FAILURE.
 *
 * @return  None.
 */
static void ubTest_scan_windowCompleteCB (bStatus_t status)
{
  scanIndCount++;

  Display_print1(dispHandle, UBT_ROW_SCAN_2, 0,
                 "%d Scan Window\'s done", scanIndCount);
}
#endif /* FEATURE_OBSERVER */

#if defined(FEATURE_MONITOR)

/* *******************************************************************
 *                     Monitor test functions
 * *******************************************************************/

/*********************************************************************
 * @fn      ubTest_monitor_stateChangeCB
 *
 * @brief   Callback from Micro Monitor indicating a state change.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
void ubTest_monitor_stateChangeCB(ugapMonitorState_t newState)
{
  switch (newState)
  {
  case UGAP_MONITOR_STATE_INITIALIZED:
    Display_print0(dispHandle, UBT_ROW_MONITOR_STATE, 0,
                   "Monitor_State: Initialized");
    break;

  case UGAP_MONITOR_STATE_IDLE:
    Display_print0(dispHandle, UBT_ROW_MONITOR_STATE, 0,
                   "Monitor_State: Idle");
    break;

  case UGAP_MONITOR_STATE_MONITORING:
    Display_print0(dispHandle, UBT_ROW_MONITOR_STATE, 0,
                   "Monitor_State: Monitoring");
    break;

  default:
    break;
  }
}

/*********************************************************************
 * @fn      ubTest_monitor_indicationCB
 *
 * @brief   Callback from Micro monitor notifying that a advertising
 *          packet is received.
 *
 * @param   status status of a monitoring scan
 * @param   sessionId session ID
 * @param   len length of the payload
 * @param   pPayload pointer to payload
 *
 * @return  None.
 */
void ubTest_monitor_indicationCB(bStatus_t status, uint8_t sessionId, uint8_t len, uint8_t *pPayload)
{
  static int8   rssi;
  static uint8  chan;
  static uint32 timeStamp;

  /* We have a packet (dvertisment packek as an example):
   *
   * | Preamble  | Access Addr | PDU         | CRC     |
   * | 1-2 bytes | 4 bytes     | 2-257 bytes | 3 bytes |
   *
   * The PDU is expended to:
   * | Header  | Payload     |
   * | 2 bytes | 1-255 bytes |
   *
   * The Header is expended to:
   * | PDU Type...RxAdd | Length |
   * | 1 byte           | 1 byte |
   *
   * The Payload is expended to:
   * | AdvA    | AdvData    |
   * | 6 bytes | 0-31 bytes |
   *
   * The radio stripps the CRC and replaces it with the postfix.
   *
   * The Postfix is expended to:
   * | RSSI   | Status | TimeStamp |
   * | 1 byte | 1 byte | 4 bytes   |
   *
   * The Status is expended to:
   * | bCrcErr | bIgnore | channel  |
   * | bit 7   | bit 6   | bit 5..0 |
   *
   * Note the advPke is the beginning of PDU; the dataLen includes
   * the postfix length.
   *
   */
  chan = (*(pPayload + len - 5) & 0x3F);
  rssi = *(pPayload + len - 6);
  timeStamp = *(uint32 *)(pPayload + len - 4);
  Display_print5(dispHandle, UBT_ROW_MONITOR_1, 0, "MonitorInd M/S: %d sessionId: %d Chan: %d RSSI: %d timeStamp: %ul", status, sessionId, chan, rssi, timeStamp);
}

/*********************************************************************
 * @fn      ubTest_monitor_completeCB
 *
 * @brief   Callback from Micro Monitor notifying that a monitoring
 *          scan is completed.
 *
 * @param   status - How the last event was done. SUCCESS or FAILURE.
 * @param   sessionId - Session ID
 *
 * @return  None.
 */
void ubTest_monitor_completeCB (bStatus_t status, uint8_t sessionId)
{
  monitorCompleteCount++;
  Display_print3(dispHandle, UBT_ROW_MONITOR_2, 0,
                 "%d Monitor duration\'s done. sessionId: %d, Status: %d", monitorCompleteCount, sessionId, status);
}
#endif /* FEATURE_MONITOR */

#endif /* !POWER_TEST */

/*********************************************************************
 *********************************************************************/
