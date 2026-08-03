PROJECT := stm32f103_sk6812_hue_cycle
BUILD := build

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

CPU := -mcpu=cortex-m3 -mthumb
CFLAGS := $(CPU) -Os -g3 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -std=gnu11
ASFLAGS := $(CPU) -g3
LDFLAGS := $(CPU) -Tsrc/stm32f103_medium.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(PROJECT).map

OBJS := $(BUILD)/main.o $(BUILD)/startup_stm32f103.o

.PHONY: all flash clean

all: $(BUILD)/$(PROJECT).elf $(BUILD)/$(PROJECT).bin
	$(SIZE) $(BUILD)/$(PROJECT).elf

$(BUILD)/$(PROJECT).elf: $(OBJS) src/stm32f103_medium.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/main.o: src/main.c
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD)' | Out-Null"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup_stm32f103.o: src/startup_stm32f103.s
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD)' | Out-Null"
	$(CC) $(ASFLAGS) -c $< -o $@

flash: all
	openocd -f interface/stlink.cfg -c "hla_vid_pid 0x0483 0x3748" -c "set CPUTAPID 0x1ba01477" -f target/stm32f1x.cfg -c "adapter speed 500" -c "program $(BUILD)/$(PROJECT).elf verify reset exit"

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD)' -ErrorAction SilentlyContinue"
