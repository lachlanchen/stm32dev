APP ?= sensor

ifeq ($(APP),nand_diag)
PROJECT := stm32_nand_diag_lcd
BUILD_DIR := build_nand_diag
APP_SRCS := src/nand_diag_main.c src/nand_diag_i18n.c
else ifeq ($(APP),qspi_diag)
PROJECT := stm32_qspi_diag
BUILD_DIR := build_qspi_diag
APP_SRCS := src/qspi_diag_main.c
else ifeq ($(APP),sk6812)
PROJECT := stm32_sk6812_rgbw
BUILD_DIR := build_sk6812
APP_SRCS := src/sk6812_rgbw_main.c
else ifeq ($(APP),sk6812_pa3_test)
PROJECT := stm32_sk6812_pa3_test
BUILD_DIR := build_sk6812_pa3_test
APP_SRCS := src/sk6812_pa3_test_main.c
else ifeq ($(APP),sk6812_pa3_timer_test)
PROJECT := stm32_sk6812_pa3_timer_test
BUILD_DIR := build_sk6812_pa3_timer_test
APP_SRCS := src/sk6812_pa3_timer_test_main.c
else ifeq ($(APP),sk6812_pa3_smooth_demo)
PROJECT := stm32_sk6812_pa3_smooth_demo
BUILD_DIR := build_sk6812_pa3_smooth_demo
APP_SRCS := src/sk6812_pa3_smooth_demo_main.c
else ifeq ($(APP),ws2812b_pa3_smooth_demo)
PROJECT := stm32_ws2812b_pa3_smooth_demo
BUILD_DIR := build_ws2812b_pa3_smooth_demo
APP_SRCS := src/ws2812b_pa3_smooth_demo_main.c
else ifeq ($(APP),tungsten_pa2_smooth_test)
PROJECT := stm32_tungsten_pa2_smooth_test
BUILD_DIR := build_tungsten_pa2_smooth_test
APP_SRCS := src/tungsten_pa2_smooth_test_main.c
else ifeq ($(APP),tungsten_pa2_full_diagnostic)
PROJECT := stm32_tungsten_pa2_full_diagnostic
BUILD_DIR := build_tungsten_pa2_full_diagnostic
APP_SRCS := src/tungsten_pa2_full_diagnostic_main.c
else ifeq ($(APP),tungsten_pa2_6vrms_monitor)
PROJECT := stm32_tungsten_pa2_6vrms_monitor
BUILD_DIR := build_tungsten_pa2_6vrms_monitor
APP_SRCS := src/tungsten_pa2_6vrms_monitor_main.c
else ifeq ($(APP),tungsten_pa2_8vrms_pulse)
PROJECT := stm32_tungsten_pa2_8vrms_pulse
BUILD_DIR := build_tungsten_pa2_8vrms_pulse
APP_SRCS := src/tungsten_pa2_8vrms_pulse_main.c
else
PROJECT := stm32_sensor_head_lcd
BUILD_DIR := build
APP_SRCS := src/main.c
endif

VENDOR := vendor
SRC := src

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

CPU_FLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
DEFS := -DUSE_HAL_DRIVER -DSTM32H743xx
INCS := \
  -I$(SRC) \
  -I$(VENDOR)/CORE \
  -I$(VENDOR)/USER \
  -I$(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Inc \
  -I$(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Inc/Legacy \
  -I$(VENDOR)/SYSTEM/sys \
  -I$(VENDOR)/SYSTEM/delay \
  -I$(VENDOR)/SYSTEM/usart \
  -I$(VENDOR)/HARDWARE/LCD \
  -I$(VENDOR)/HARDWARE/SDRAM

CFLAGS := $(CPU_FLAGS) $(DEFS) $(INCS) -Os -g3 -ffunction-sections -fdata-sections -Wall -Wno-unused-parameter -Wno-missing-braces -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -std=gnu11
ASFLAGS := $(CPU_FLAGS) -g3
LDFLAGS := $(CPU_FLAGS) -T$(SRC)/stm32h743_flash.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

SRCS := \
  $(SRC)/startup_stm32h743xx_gcc.s \
  $(APP_SRCS) \
  $(SRC)/delay.c \
  $(SRC)/usart.c \
  $(VENDOR)/USER/system_stm32h7xx.c \
  $(VENDOR)/SYSTEM/sys/sys.c \
  $(VENDOR)/HARDWARE/SDRAM/sdram.c \
  $(VENDOR)/HARDWARE/LCD/lcd.c \
  $(VENDOR)/HARDWARE/LCD/ltdc.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ltdc.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_qspi.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdram.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sram.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmc.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRCS))) $(patsubst %.s,$(BUILD_DIR)/%.o,$(filter %.s,$(SRCS)))

.PHONY: all clean flash nand-diag flash-nand-diag clean-nand-diag qspi-diag flash-qspi-diag clean-qspi-diag sk6812 flash-sk6812 clean-sk6812 sk6812-pa3-test flash-sk6812-pa3-test clean-sk6812-pa3-test sk6812-pa3-timer-test flash-sk6812-pa3-timer-test clean-sk6812-pa3-timer-test sk6812-pa3-smooth-demo flash-sk6812-pa3-smooth-demo clean-sk6812-pa3-smooth-demo ws2812b-pa3-smooth-demo flash-ws2812b-pa3-smooth-demo clean-ws2812b-pa3-smooth-demo tungsten-pa2-smooth-test flash-tungsten-pa2-smooth-test clean-tungsten-pa2-smooth-test tungsten-pa2-full-diagnostic flash-tungsten-pa2-full-diagnostic clean-tungsten-pa2-full-diagnostic tungsten-pa2-6vrms-monitor flash-tungsten-pa2-6vrms-monitor clean-tungsten-pa2-6vrms-monitor tungsten-pa2-8vrms-pulse flash-tungsten-pa2-8vrms-pulse clean-tungsten-pa2-8vrms-pulse

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin
	$(SIZE) $(BUILD_DIR)/$(PROJECT).elf

$(BUILD_DIR)/$(PROJECT).elf: $(OBJS) $(SRC)/stm32h743_flash.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(dir $@)' | Out-Null"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(dir $@)' | Out-Null"
	$(CC) $(ASFLAGS) -c $< -o $@

flash: all
	openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"

nand-diag:
	$(MAKE) APP=nand_diag all

flash-nand-diag:
	$(MAKE) APP=nand_diag flash

clean-nand-diag:
	$(MAKE) APP=nand_diag clean

qspi-diag:
	$(MAKE) APP=qspi_diag all

flash-qspi-diag:
	$(MAKE) APP=qspi_diag flash

clean-qspi-diag:
	$(MAKE) APP=qspi_diag clean

sk6812:
	$(MAKE) APP=sk6812 all

flash-sk6812:
	$(MAKE) APP=sk6812 flash

clean-sk6812:
	$(MAKE) APP=sk6812 clean

sk6812-pa3-test:
	$(MAKE) APP=sk6812_pa3_test all

flash-sk6812-pa3-test: sk6812-pa3-test
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_sk6812_pa3_test/stm32_sk6812_pa3_test.elf verify reset exit"

clean-sk6812-pa3-test:
	$(MAKE) APP=sk6812_pa3_test clean

sk6812-pa3-timer-test:
	$(MAKE) APP=sk6812_pa3_timer_test all

flash-sk6812-pa3-timer-test: sk6812-pa3-timer-test
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_sk6812_pa3_timer_test/stm32_sk6812_pa3_timer_test.elf verify reset exit"

clean-sk6812-pa3-timer-test:
	$(MAKE) APP=sk6812_pa3_timer_test clean

sk6812-pa3-smooth-demo:
	$(MAKE) APP=sk6812_pa3_smooth_demo all

flash-sk6812-pa3-smooth-demo: sk6812-pa3-smooth-demo
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_sk6812_pa3_smooth_demo/stm32_sk6812_pa3_smooth_demo.elf verify reset exit"

clean-sk6812-pa3-smooth-demo:
	$(MAKE) APP=sk6812_pa3_smooth_demo clean

ws2812b-pa3-smooth-demo:
	$(MAKE) APP=ws2812b_pa3_smooth_demo all

flash-ws2812b-pa3-smooth-demo: ws2812b-pa3-smooth-demo
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_ws2812b_pa3_smooth_demo/stm32_ws2812b_pa3_smooth_demo.elf verify reset exit"

clean-ws2812b-pa3-smooth-demo:
	$(MAKE) APP=ws2812b_pa3_smooth_demo clean

tungsten-pa2-smooth-test:
	$(MAKE) APP=tungsten_pa2_smooth_test all

flash-tungsten-pa2-smooth-test: tungsten-pa2-smooth-test
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_tungsten_pa2_smooth_test/stm32_tungsten_pa2_smooth_test.elf verify reset exit"

clean-tungsten-pa2-smooth-test:
	$(MAKE) APP=tungsten_pa2_smooth_test clean

tungsten-pa2-full-diagnostic:
	$(MAKE) APP=tungsten_pa2_full_diagnostic all

flash-tungsten-pa2-full-diagnostic: tungsten-pa2-full-diagnostic
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_tungsten_pa2_full_diagnostic/stm32_tungsten_pa2_full_diagnostic.elf verify reset exit"

clean-tungsten-pa2-full-diagnostic:
	$(MAKE) APP=tungsten_pa2_full_diagnostic clean

tungsten-pa2-6vrms-monitor:
	$(MAKE) APP=tungsten_pa2_6vrms_monitor all

flash-tungsten-pa2-6vrms-monitor: tungsten-pa2-6vrms-monitor
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_tungsten_pa2_6vrms_monitor/stm32_tungsten_pa2_6vrms_monitor.elf verify reset exit"

clean-tungsten-pa2-6vrms-monitor:
	$(MAKE) APP=tungsten_pa2_6vrms_monitor clean

tungsten-pa2-8vrms-pulse:
	$(MAKE) APP=tungsten_pa2_8vrms_pulse all

flash-tungsten-pa2-8vrms-pulse: tungsten-pa2-8vrms-pulse
	openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program build_tungsten_pa2_8vrms_pulse/stm32_tungsten_pa2_8vrms_pulse.elf verify reset exit"

clean-tungsten-pa2-8vrms-pulse:
	$(MAKE) APP=tungsten_pa2_8vrms_pulse clean

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue"
