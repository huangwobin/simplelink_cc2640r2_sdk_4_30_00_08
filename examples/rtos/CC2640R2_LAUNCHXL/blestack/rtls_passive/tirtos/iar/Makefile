PROJECT=$(BLE_REPO)/examples/rtos/CC2640R2_LAUNCHXL/blestack/rtls_passive/tirtos/iar
BLE_REPO=../../../../../../..
IAR_PATH=C:/Program\ Files\ \(x86\)/IAR\ Systems/EWARM-$(IAR_VERSION)/common/bin
IAR_VERSION=8.32.2

all: rtls_passive

rtls_passive:
	$(IAR_PATH)/iarbuild.exe $(PROJECT)/app/cc2640r2lp_app.ewp \
	-build \
	FlashOnly \
	-log info \
	-parallel 1 \
	-varfile rtls_passive.custom_argvars

clean:
	$(IAR_PATH)/iarbuild.exe $(PROJECT)/app/cc2640r2lp_app.ewp \
	-clean \
	FlashOnly \
	-log info \
	-parallel 1 \
	-varfile rtls_passive.custom_argvars
