#include <bcomdef.h>
#include <ti/display/Display.h>
#include <menu/two_btn_menu.h>
#include "micro_ble_test_menu.h"
#include "micro_ble_test.h"

/*
 * Menu Lists Initializations
 */

/* Menu: Main
     8 submenus, no upper */
MENU_OBJ(ubtMenuMain, "Main Menu", 8, NULL)
  MENU_ITEM_SUBMENU(&ubtMenuInit)
  MENU_ITEM_SUBMENU(&ubtMenuTxPower)
  MENU_ITEM_SUBMENU(&ubtMenuAdvInterval)
  MENU_ITEM_SUBMENU(&ubtMenuAdvChanMap)
  MENU_ITEM_SUBMENU(&ubtMenuTimeToPrepare)
  MENU_ITEM_SUBMENU(&ubtMenuAdvData)
  MENU_ITEM_SUBMENU(&ubtMenuBcastDuty)
  MENU_ITEM_SUBMENU(&ubtMenuBcastControl)
MENU_OBJ_END

/* Menu: Init
     2 items(1 action & 1 submenus), upper = ubtMenuMain */
MENU_OBJ(ubtMenuInit, "Stack&Bcast Init", 2, &ubtMenuMain)
  MENU_ITEM_ACTION("Use PublicAddr", ubTest_doInitAddrPublic)
  MENU_ITEM_SUBMENU(&ubtMenuAddrStatic)
MENU_OBJ_END

/* Menu: Init - AddrStatic
     4 items(4 actions), upper = ubtMenuInit */
MENU_OBJ(ubtMenuAddrStatic, "Use RandStaticAddr", 4, &ubtMenuInit)
  MENU_ITEM_ACTION("Generate RandAddr", ubTest_doInitAddrStaticGenerate)
  MENU_ITEM_ACTION("Use C0FFEEC0FFEE", ubTest_doInitAddrStaticSelect)
  MENU_ITEM_ACTION("Use DABBDABBDABB", ubTest_doInitAddrStaticSelect)
  MENU_ITEM_ACTION("Use FEEDCAFEF00D", ubTest_doInitAddrStaticSelect)
MENU_OBJ_END

/* Menu: TX Power */
MENU_OBJ(ubtMenuTxPower, "Set TX Power", 2, &ubtMenuMain)
  MENU_ITEM_ACTION("Inc TX Power", ubTest_doTxPower)
  MENU_ITEM_ACTION("Dec TX Power", ubTest_doTxPower)
MENU_OBJ_END

/* Menu: Adv Interval */
MENU_OBJ(ubtMenuAdvInterval, "Set Adv Interval", 2, &ubtMenuMain)
  MENU_ITEM_ACTION("Inc Adv Interval", ubTest_doAdvInterval)
  MENU_ITEM_ACTION("Dec Adv Interval", ubTest_doAdvInterval)
MENU_OBJ_END

/* Menu: Adv Channel Map */
MENU_OBJ(ubtMenuAdvChanMap, "Set Adv Chan Map", 2, &ubtMenuMain)
  MENU_ITEM_ACTION("Inc Adv Chan Map", ubTest_doAdvChanMap)
  MENU_ITEM_ACTION("Dec Adv Chan Map", ubTest_doAdvChanMap)
MENU_OBJ_END

/* Menu: Time to Adv */
MENU_OBJ(ubtMenuTimeToPrepare, "Set TimeToPrepare", 2, &ubtMenuMain)
  MENU_ITEM_ACTION("Inc TimeToPrepare", ubTest_doTimeToPrepare)
  MENU_ITEM_ACTION("Dec TimeToPrepare", ubTest_doTimeToPrepare)
MENU_OBJ_END

/* Menu: Adv Data */
MENU_OBJ(ubtMenuAdvData, "Set Adv Data", 5, &ubtMenuMain)
  MENU_ITEM_SUBMENU(&ubtMenuUpdateOption)
  MENU_ITEM_ACTION("Eddystone UID", ubTest_doSetFrameType)
  MENU_ITEM_ACTION("Eddystone URL", ubTest_doSetFrameType)
  MENU_ITEM_ACTION("Eddystone TLM", ubTest_doSetFrameType)
  MENU_ITEM_SUBMENU(&ubtMenuTimeToPrepare)
MENU_OBJ_END

/* Menu: Update Option */
MENU_OBJ(ubtMenuUpdateOption, "Update Option", 2, &ubtMenuAdvData)
  MENU_ITEM_ACTION("Immediately", ubTest_doUpdateOption)
  MENU_ITEM_ACTION("Upon Prepare Evt", ubTest_doUpdateOption)
MENU_OBJ_END

/* Menu: Bcast Duty */
MENU_OBJ(ubtMenuBcastDuty, "Bcast Duty Control", 2, &ubtMenuMain)
  MENU_ITEM_SUBMENU(&ubtMenuBcastDutyOnTime)
  MENU_ITEM_SUBMENU(&ubtMenuBcastDutyOffTime)
MENU_OBJ_END

/* Menu: Bcast Duty - On Time */
MENU_OBJ(ubtMenuBcastDutyOnTime, "Set Duty On Time", 3, &ubtMenuBcastDuty)
  MENU_ITEM_ACTION("Inc Duty On Time", ubTest_doBcastDutyOnTime)
  MENU_ITEM_ACTION("Dec Duty On Time", ubTest_doBcastDutyOnTime)
  MENU_ITEM_SUBMENU(&ubtMenuMain)
MENU_OBJ_END

/* Menu: Bcast Duty - Off Time */
MENU_OBJ(ubtMenuBcastDutyOffTime, "Set Duty Off Time", 3, &ubtMenuBcastDuty)
  MENU_ITEM_ACTION("Inc Duty Off Time", ubTest_doBcastDutyOffTime)
  MENU_ITEM_ACTION("Dec Duty Off Time", ubTest_doBcastDutyOffTime)
  MENU_ITEM_SUBMENU(&ubtMenuMain)
MENU_OBJ_END

/* Menu: Bcast Control */
MENU_OBJ(ubtMenuBcastControl, "Bcast Start/Stop", 3, &ubtMenuMain)
  MENU_ITEM_ACTION("Start(Indef. Adv)", ubTest_doBcastStart)
  MENU_ITEM_ACTION("Start(100 Adv\'s)", ubTest_doBcastStart)
  MENU_ITEM_ACTION("Stop", ubTest_doBcastStop)
MENU_OBJ_END

