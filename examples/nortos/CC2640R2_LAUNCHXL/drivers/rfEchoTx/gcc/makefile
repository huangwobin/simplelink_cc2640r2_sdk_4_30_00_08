COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR ?= $(abspath ../../../../../..)

include $(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/imports.mak

CC = "$(GCC_ARMCOMPILER)/bin/arm-none-eabi-gcc"
LNK = "$(GCC_ARMCOMPILER)/bin/arm-none-eabi-gcc"

OBJECTS = RFQueue.obj main_nortos.obj smartrf_settings.obj rfEchoTx.obj CC2640R2_LAUNCHXL.obj CC2640R2_LAUNCHXL_fxns.obj ccfg.obj

NAME = rfEchoTx

CFLAGS = -I.. \
    -DDeviceFamily_CC26X0R2 \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source" \
    -DSUPPORT_PHY_CUSTOM \
    -DSUPPORT_PHY_2_4_100KBPS2GFSK \
    -DSUPPORT_PHY_2_4_250KBPS2GFSK \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    -mcpu=cortex-m3 \
    -march=armv7-m \
    -mthumb \
    -std=c99 \
    -mfloat-abi=soft \
    -ffunction-sections \
    -fdata-sections \
    -g \
    -gstrict-dwarf \
    -Wall \
    "-I$(GCC_ARMCOMPILER)/arm-none-eabi/include/newlib-nano" \
    "-I$(GCC_ARMCOMPILER)/arm-none-eabi/include"

LFLAGS = -Wl,-T,../gcc/CC2640R2_LAUNCHXL_NoRTOS.lds \
    "-Wl,-Map,$(NAME).map" \
    "-L$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/source" \
    -l:ti/display/lib/display.am3g \
    -l:ti/grlib/lib/gcc/m3/grlib.a \
    -l:third_party/spiffs/lib/gcc/m3/spiffs_cc26xx.a \
    -l:ti/drivers/rf/lib/rf_multiMode_cc26x0r2.am3g \
    -l:ti/drivers/lib/drivers_cc26x0r2.am3g \
    -eresetISR \
    "-L$(COM_TI_SIMPLELINK_CC2640R2_SDK_INSTALL_DIR)/kernel/nortos" \
    -l:lib/nortos_cc26x0r2.am3g \
    -l:ti/devices/cc26x0r2/driverlib/bin/gcc/driverlib.lib \
    -march=armv7-m \
    -mthumb \
    -nostartfiles \
    -static \
    -Wl,--gc-sections \
    "-L$(GCC_ARMCOMPILER)/arm-none-eabi/lib/thumb/v7-m" \
    "-L$(GCC_ARMCOMPILER)/arm-none-eabi/lib" \
    -lgcc \
    -lc \
    -lm \
    -lnosys \
    --specs=nano.specs

all: $(NAME).out

RFQueue.obj: ../RFQueue.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

main_nortos.obj: ../main_nortos.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

smartrf_settings.obj: ../smartrf_settings/smartrf_settings.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

rfEchoTx.obj: ../rfEchoTx.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

CC2640R2_LAUNCHXL.obj: ../CC2640R2_LAUNCHXL.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

CC2640R2_LAUNCHXL_fxns.obj: ../CC2640R2_LAUNCHXL_fxns.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

ccfg.obj: ../ccfg.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -c -o $@

$(NAME).out: $(OBJECTS)
	@ echo linking $@
	@ $(LNK)  $(OBJECTS) $(LFLAGS) -o $(NAME).out

clean:
	@ echo Cleaning...
	@ $(RM) $(OBJECTS) > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).out > $(DEVNULL) 2>&1
	@ $(RM) $(NAME).map > $(DEVNULL) 2>&1
