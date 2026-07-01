PROJECT := stm32_sensor_head_lcd
BUILD_DIR := build
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
  $(SRC)/main.c \
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
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdram.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sram.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c \
  $(VENDOR)/HALLIB/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmc.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRCS))) $(patsubst %.s,$(BUILD_DIR)/%.o,$(filter %.s,$(SRCS)))

.PHONY: all clean flash

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

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue"
