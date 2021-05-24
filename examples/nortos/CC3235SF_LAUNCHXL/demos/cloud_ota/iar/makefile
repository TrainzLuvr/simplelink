SIMPLELINK_CC32XX_SDK_INSTALL_DIR ?= $(abspath ../../../../../..)

include $(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/imports.mak


SYSCONFIG_PRODUCT := $(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/.metadata/product.json

CC = "$(IAR_ARMCOMPILER)/bin/iccarm"
LNK = "$(IAR_ARMCOMPILER)/bin/ilinkarm"

OBJECTS = ti_drivers_config.obj ti_drivers_net_wifi_config.obj uart_term.obj main_nortos.obj mqueue_nortos.obj cloud_ota.obj platform.obj

NAME = cloud_ota

CFLAGS = -I.. \
    -I. \
    -DCC32XX \
    "-I$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source" \
    -DNORTOS_SUPPORT \
    "-I$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/kernel/nortos" \
    "-I$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/kernel/nortos/posix" \
    --cpu=Cortex-M4 \
    --debug \
    --silent \
    -e \
    --aeabi \
    --thumb \
    --diag_suppress=Pa050

LFLAGS = "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/net/ota/iar/Release/ota.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/utils/json/lib/iar/m4/json_release.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/display/lib/display.arm4" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/grlib/lib/iar/m4/grlib.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/third_party/spiffs/lib/iar/m4/spiffs.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/drivers/lib/drivers_cc32xx.arm4" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/third_party/fatfs/lib/iar/m4/fatfs.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/drivers/net/wifi/iar/nortos/simplelink.a" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/kernel/nortos/lib/nortos_cc32xx.arm4" \
    "$(SIMPLELINK_CC32XX_SDK_INSTALL_DIR)/source/ti/devices/cc32xx/driverlib/iar/Release/driverlib.a" \
    --config ../iar/CC3235SF_LAUNCHXL_NoRTOS.icf \
    --map "$(NAME).map" \
    --silent \
    --cpu=Cortex-M4 \
    --semihosting=iar_breakpoint

all: $(NAME).out

.INTERMEDIATE: syscfg
ti_drivers_config.c ti_drivers_config.h ti_drivers_net_wifi_config.c: syscfg
	@ echo generation complete

syscfg: ../common_nortos.syscfg
	@ echo Generating configuration files...
	@ $(NODE_JS) $(SYSCONFIG_TOOL) --product $(SYSCONFIG_PRODUCT) --output $(@D) $<

ti_drivers_config.obj: ti_drivers_config.c
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

uart_term.obj: ../uart_term.c ti_drivers_config.h
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

main_nortos.obj: ../main_nortos.c ti_drivers_config.h
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

mqueue_nortos.obj: ../mqueue_nortos.c ti_drivers_config.h
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

cloud_ota.obj: ../cloud_ota.c ti_drivers_config.h
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

platform.obj: ../platform.c ti_drivers_config.h
	@ echo Building $@
	@ $(CC) $(CFLAGS) $< -o $@

ti_drivers_net_wifi_config.obj: ti_drivers_net_wifi_config.c ti_drivers_config.h
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
	@ $(RM) ti_drivers_config.c ti_drivers_config.h ti_drivers_net_wifi_config.c > $(DEVNULL) 2>&1
