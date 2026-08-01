PROJECT := stm32f103_rgbw_spectral_controller
BUILD_DIR := build
CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

CFLAGS := -mcpu=cortex-m3 -mthumb -Os -ffunction-sections -fdata-sections \
	-Wall -Wextra -Werror -std=c11
LDFLAGS := -T src/stm32f103_medium.ld -nostartfiles -Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/$(PROJECT).map

SOURCES := src/startup_stm32f103.s src/main.c
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SOURCES))) \
	$(patsubst src/%.s,$(BUILD_DIR)/%.o,$(filter %.s,$(SOURCES)))

.PHONY: all flash clean

all: $(BUILD_DIR)/$(PROJECT).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

flash: $(BUILD_DIR)/$(PROJECT).elf
	openocd -f interface/stlink.cfg \
		-c "hla_vid_pid 0x0483 0x3748" \
		-f target/stm32f1x.cfg \
		-c "adapter speed 1000" \
		-c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"

clean:
	rm -rf $(BUILD_DIR)
