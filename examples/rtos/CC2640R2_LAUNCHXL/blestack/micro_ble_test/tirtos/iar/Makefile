PROJECT=$(BLE_REPO)/examples/rtos/CC2640R2_LAUNCHXL/blestack/micro_ble_test/tirtos/iar
BLE_REPO=../../../../../../..
IAR_PATH=C:/Program\ Files\ \(x86\)/IAR\ Systems/EWARM-$(IAR_VERSION)/common/bin
IAR_VERSION=8.32.2

all: micro_ble_test

release: clean micro_ble_test

micro_ble_test:
	$(IAR_PATH)/iarbuild.exe $(PROJECT)/app/cc2640r2lp_app.ewp \
	-build \
	FlashOnly \
	-log info \
	-parallel 1 \
	-varfile micro_ble_test.custom_argvars

clean:
	$(IAR_PATH)/iarbuild.exe $(PROJECT)/app/cc2640r2lp_app.ewp \
	-clean \
	FlashOnly \
	-log info \
	-parallel 1 \
	-varfile micro_ble_test.custom_argvars
