COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR ?= $(abspath ../../../../../..)

include $(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/imports.mak

CC = "$(IAR_ARMCOMPILER)/bin/iccarm"
LNK = "$(IAR_ARMCOMPILER)/bin/ilinkarm"

OBJECTS = RFQueue.obj main_nortos.obj smartrf_settings.obj smartrf_settings_ble.obj smartrf_settings_predefined.obj config.obj menu_nortos.obj rfPacketErrorRate_nortos.obj rx.obj tx.obj CC2640R2_LAUNCHXL.obj CC2640R2_LAUNCHXL_fxns.obj ccfg.obj

NAME = rfPacketErrorRate

CFLAGS = -I.. \
    -DBOARD_DISPLAY_USE_LCD=1 \
    -DDeviceFamily_CC26X0R2 \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source" \
    -DSUPPORT_PHY_CUSTOM \
    -DSUPPORT_PHY_2_4_100KBPS2GFSK \
    -DSUPPORT_PHY_2_4_250KBPS2GFSK \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    --cpu=Cortex-M3 \
    --debug \
    --silent \
    -e \
    --aeabi \
    --thumb \
    --diag_suppress=Pa050

LFLAGS = "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/display/lib/display.arm3" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/grlib/lib/iar/m3/grlib.a" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/third_party/spiffs/lib/iar/m3/spiffs_cc26xx.a" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/drivers/rf/lib/rf_multiMode_cc26x0r2.arm3" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/drivers/lib/drivers_cc26x0r2.arm3" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos/lib/nortos_cc26x0r2.arm3" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/devices/cc26x0r2/driverlib/bin/iar/driverlib.lib" \
    --config ../iar/CC2640R2_LAUNCHXL_NoRTOS.icf \
    --map "$(NAME).map" \
    --silent \
    --cpu=Cortex-M3 \
    --semihosting=iar_breakpoint

all: $(NAME).out

RFQueue.obj: ../RFQueue.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

main_nortos.obj: ../main_nortos.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

smartrf_settings.obj: ../smartrf_settings/smartrf_settings.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

smartrf_settings_ble.obj: ../smartrf_settings/smartrf_settings_ble.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

smartrf_settings_predefined.obj: ../smartrf_settings/smartrf_settings_predefined.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

config.obj: ../config.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

menu_nortos.obj: ../menu_nortos.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

rfPacketErrorRate_nortos.obj: ../rfPacketErrorRate_nortos.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

rx.obj: ../rx.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

tx.obj: ../tx.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

CC2640R2_LAUNCHXL.obj: ../CC2640R2_LAUNCHXL.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

CC2640R2_LAUNCHXL_fxns.obj: ../CC2640R2_LAUNCHXL_fxns.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

ccfg.obj: ../ccfg.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

$(NAME).out: $(OBJECTS)
	@ echo linking $@
	@ $(LNK)  $(OBJECTS) $(LFLAGS) -o $(NAME).out

clean:
	@ echo Cleaning...
	@ $(RM) $(OBJECTS) > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).out > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).map > $(DEVNULL) 2>&1
