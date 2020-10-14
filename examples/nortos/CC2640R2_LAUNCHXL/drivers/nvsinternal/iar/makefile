SIMPLELINK_CC2640R2_SDK_INSTALL_DIR ?= $(abspath ../../../../../..)

include $(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/imports.mak

CC = "$(IAR_ARMCOMPILER)/bin/iccarm"
LNK = "$(IAR_ARMCOMPILER)/bin/ilinkarm"

OBJECTS = main_nortos.obj nvsinternal.obj CC2640R2_LAUNCHXL.obj CC2640R2_LAUNCHXL_fxns.obj ccfg.obj

NAME = nvsinternal

CFLAGS = -I.. \
    -DDeviceFamily_CC26X0R2 \
    "-I$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source" \
    "-I$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    --cpu=Cortex-M3 \
    --debug \
    --silent \
    -e \
    --aeabi \
    --thumb \
    --diag_suppress=Pa050

LFLAGS = "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/display/lib/display.arm3" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/grlib/lib/iar/m3/grlib.a" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/third_party/spiffs/lib/iar/m3/spiffs_cc26xx.a" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/drivers/rf/lib/rf_multiMode_cc26x0r2.arm3" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/drivers/lib/drivers_cc26x0r2.arm3" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos/lib/nortos_cc26x0r2.arm3" \
    "$(SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/devices/cc26x0r2/driverlib/bin/iar/driverlib.lib" \
    --config ../iar/CC2640R2_LAUNCHXL_NoRTOS.icf \
    --map "$(NAME).map" \
    --silent \
    --cpu=Cortex-M3 \
    --semihosting=iar_breakpoint

all: $(NAME).out

main_nortos.obj: ../main_nortos.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

nvsinternal.obj: ../nvsinternal.c
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
