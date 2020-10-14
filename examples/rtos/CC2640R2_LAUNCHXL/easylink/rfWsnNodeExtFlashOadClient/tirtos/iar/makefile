COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR ?= $(abspath ../../../../../../..)

include $(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/imports.mak

XDCPATH = $(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source;$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/tirtos/packages;

CC = "$(IAR_ARMCOMPILER)/bin/iccarm"
LNK = "$(IAR_ARMCOMPILER)/bin/ilinkarm"

XDCTARGET = iar.targets.arm.M3
PLATFORM = ti.platforms.simplelink:CC2640R2F

OBJECTS = scif.obj oad_client.obj oad_protocol.obj oad_storage.obj oad_target_external_flash.obj SceAdc.obj easylink_config.obj EasyLink.obj ExtFlash.obj smartrf_settings.obj smartrf_settings_predefined.obj NodeRadioTask.obj NodeTask.obj rfWsnNode.obj scif_framework.obj scif_osal_tirtos.obj CC2640R2_LAUNCHXL.obj CC2640R2_LAUNCHXL_fxns.obj

CONFIGPKG = rfWsnNodeExtFlashOadClient

NAME = rfWsnNodeExtFlashOadClient

CFLAGS = -I../.. \
    -DOAD_BLOCK_SIZE=64 \
    -DSUPPORT_PHY_CUSTOM \
    -DSUPPORT_PHY_2_4_100KBPS2GFSK \
    -DSUPPORT_PHY_2_4_250KBPS2GFSK \
    -DDeviceFamily_CC26X0R2 \
    -DCCFG_FORCE_VDDR_HH=0 \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source" \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/posix/iar" \
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
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/tirtos/packages/ti/dpl/lib/dpl_cc26x0r2.arm3" \
    "$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source/ti/devices/cc26x0r2/driverlib/bin/iar/driverlib.lib" \
    --config ../../tirtos/iar/CC2640R2_LAUNCHXL_TIRTOS_OAD.icf \
    --map "$(NAME).map" \
    --silent \
    --cpu=Cortex-M3 \
    --semihosting=iar_breakpoint

.PRECIOUS: $(CONFIGPKG)/compiler.opt $(CONFIGPKG)/linker.cmd

all: $(NAME).out

$(CONFIGPKG)/compiler.opt: $(CONFIGPKG)/linker.cmd

$(CONFIGPKG)/linker.cmd: ../../rfWsnNodeExtFlashOadClient.cfg
	@ echo Running Configuro...
	@ $(XDC_INSTALL_DIR)/xs --xdcpath="$(XDCPATH)" xdc.tools.configuro -c "$(IAR_ARMCOMPILER)" -t $(XDCTARGET) -p $(PLATFORM) --compileOptions "$(CFLAGS)" -o "$(CONFIGPKG)" ../../rfWsnNodeExtFlashOadClient.cfg

scif.obj: ../../sce/scif.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

oad_client.obj: ../../oad/native_oad/oad_client.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

oad_protocol.obj: ../../oad/native_oad/oad_protocol.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

oad_storage.obj: ../../oad/native_oad/oad_storage.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

oad_target_external_flash.obj: ../../oad/native_oad/oad_target_external_flash.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

SceAdc.obj: ../../SceAdc.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

easylink_config.obj: ../../easylink/easylink_config.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

EasyLink.obj: ../../easylink/EasyLink.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

ExtFlash.obj: ../../extflash/ExtFlash.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

smartrf_settings.obj: ../../smartrf_settings/smartrf_settings.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

smartrf_settings_predefined.obj: ../../smartrf_settings/smartrf_settings_predefined.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

NodeRadioTask.obj: ../../NodeRadioTask.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

NodeTask.obj: ../../NodeTask.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

rfWsnNode.obj: ../../rfWsnNode.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

scif_framework.obj: ../../sce/scif_framework.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

scif_osal_tirtos.obj: ../../sce/scif_osal_tirtos.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

CC2640R2_LAUNCHXL.obj: ../../CC2640R2_LAUNCHXL.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

CC2640R2_LAUNCHXL_fxns.obj: ../../CC2640R2_LAUNCHXL_fxns.c $(CONFIGPKG)/compiler.opt
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -f $(CONFIGPKG)/compiler.opt -o $@

$(NAME).out: $(OBJECTS) $(CONFIGPKG)/linker.cmd
	@ echo linking $@
	@ $(LNK)  $(OBJECTS) -f $(CONFIGPKG)/linker.cmd $(LFLAGS) -o $(NAME).out

clean:
	@ echo Cleaning...
	@ $(RM) $(OBJECTS) > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).out > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).map > $(DEVNULL) 2>&1
	@ $(RMDIR) $(NAME) > $(DEVNULL) 2>&1
