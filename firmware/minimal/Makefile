PROJECT := minimal
BUILD_DIR := build

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

CPU_FLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
CFLAGS := $(CPU_FLAGS) -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra -Iinclude
ASFLAGS := $(CPU_FLAGS) -g3
LDFLAGS := $(CPU_FLAGS) -Tstm32h7_flash.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map

SRCS := src/main.c src/startup_stm32h7xx.s
OBJS := $(patsubst src/%,$(BUILD_DIR)/%,$(SRCS:.c=.o))
OBJS := $(OBJS:.s=.o)

.PHONY: all clean

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin
	$(SIZE) $(BUILD_DIR)/$(PROJECT).elf

$(BUILD_DIR)/$(PROJECT).elf: $(OBJS) stm32h7_flash.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
