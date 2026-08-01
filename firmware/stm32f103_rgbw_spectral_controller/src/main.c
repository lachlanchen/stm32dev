#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define FLASH_ACR       REG32(0x40022000u)
#define RCC_CR          REG32(0x40021000u)
#define RCC_CFGR        REG32(0x40021004u)
#define RCC_APB2ENR     REG32(0x40021018u)
#define GPIOA_CRL       REG32(0x40010800u)
#define GPIOA_BSRR      REG32(0x40010810u)
#define GPIOA_BRR       REG32(0x40010814u)
#define DEMCR           REG32(0xE000EDFCu)
#define DWT_CTRL        REG32(0xE0001000u)
#define DWT_CYCCNT      REG32(0xE0001004u)

#define PA0_MASK              (1u << 0)
#define CPU_HZ                64000000u
#define SK_BIT_CYCLES         80u
#define SK_T0H_CYCLES         22u
#define SK_T1H_CYCLES         45u
#define RESET_CYCLES          ((CPU_HZ / 1000000u) * 320u)
#define STATIC_WATCHDOG       (CPU_HZ * 2u)
#define MAILBOX_MAGIC         0x52474257u
#define PROTOCOL_VERSION      1u
#define MAX_SCAN_STATES       64u

enum {
    MODE_OFF = 0u,
    MODE_STATIC = 1u,
    MODE_SCAN = 2u
};

enum {
    STATUS_IDLE = 0u,
    STATUS_STATIC = 1u,
    STATUS_SCANNING = 2u,
    STATUS_COMPLETE = 3u,
    STATUS_ERROR = 4u
};

enum {
    ERROR_NONE = 0u,
    ERROR_BAD_MODE = 1u,
    ERROR_BAD_SCAN = 2u,
    ERROR_WATCHDOG = 4u
};

typedef struct {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t command_sequence;
    volatile uint32_t mode;
    volatile uint32_t pixel0_rgbw;
    volatile uint32_t pixel1_rgbw;
    volatile uint32_t state_count;
    volatile uint32_t dwell_us;
    volatile uint32_t repeat_count;
    volatile uint32_t applied_sequence;
    volatile uint32_t status;
    volatile uint32_t current_state;
    volatile uint32_t completed_repeats;
    volatile uint32_t heartbeat;
    volatile uint32_t error_flags;
    volatile uint32_t reserved;
    volatile uint32_t states[MAX_SCAN_STATES][2];
} rgbw_mailbox_t;

__attribute__((section(".mailbox"), used))
volatile rgbw_mailbox_t rgbw_mailbox;

static void clock_init(void)
{
    FLASH_ACR = (1u << 4) | 2u;
    RCC_CFGR = (4u << 8) | (14u << 18);
    RCC_CR |= (1u << 24);
    while ((RCC_CR & (1u << 25)) == 0u) {
    }
    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    while ((RCC_CFGR & (3u << 2)) != (2u << 2)) {
    }
}

static void timing_init(void)
{
    DEMCR |= (1u << 24);
    DWT_CYCCNT = 0u;
    DWT_CTRL |= 1u;
}

static void delay_cycles(uint32_t cycles)
{
    const uint32_t start = DWT_CYCCNT;
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) {
    }
}

static void gpio_init(void)
{
    RCC_APB2ENR |= (1u << 2);
    GPIOA_CRL = (GPIOA_CRL & ~0xFu) | 0x3u;
    GPIOA_BRR = PA0_MASK;
}

static void send_bit(uint32_t one)
{
    const uint32_t start = DWT_CYCCNT;
    const uint32_t high_cycles = one ? SK_T1H_CYCLES : SK_T0H_CYCLES;

    GPIOA_BSRR = PA0_MASK;
    while ((uint32_t)(DWT_CYCCNT - start) < high_cycles) {
    }
    GPIOA_BRR = PA0_MASK;
    while ((uint32_t)(DWT_CYCCNT - start) < SK_BIT_CYCLES) {
    }
}

static void send_byte(uint8_t value)
{
    uint32_t mask;
    for (mask = 0x80u; mask != 0u; mask >>= 1) {
        send_bit((value & mask) != 0u);
    }
}

static void send_packed_rgbw(uint32_t packed)
{
    const uint8_t red = (uint8_t)(packed & 0xFFu);
    const uint8_t green = (uint8_t)((packed >> 8) & 0xFFu);
    const uint8_t blue = (uint8_t)((packed >> 16) & 0xFFu);
    const uint8_t white = (uint8_t)((packed >> 24) & 0xFFu);

    send_byte(green);
    send_byte(red);
    send_byte(blue);
    send_byte(white);
}

static void show(uint32_t pixel0, uint32_t pixel1)
{
    send_packed_rgbw(pixel0);
    send_packed_rgbw(pixel1);
    GPIOA_BRR = PA0_MASK;
    delay_cycles(RESET_CYCLES);
}

static void all_off(void)
{
    show(0u, 0u);
}

static void mailbox_init(void)
{
    rgbw_mailbox.magic = MAILBOX_MAGIC;
    rgbw_mailbox.version = PROTOCOL_VERSION;
    rgbw_mailbox.command_sequence = 0u;
    rgbw_mailbox.mode = MODE_OFF;
    rgbw_mailbox.pixel0_rgbw = 0u;
    rgbw_mailbox.pixel1_rgbw = 0u;
    rgbw_mailbox.state_count = 0u;
    rgbw_mailbox.dwell_us = 0u;
    rgbw_mailbox.repeat_count = 0u;
    rgbw_mailbox.applied_sequence = 0u;
    rgbw_mailbox.status = STATUS_IDLE;
    rgbw_mailbox.current_state = 0u;
    rgbw_mailbox.completed_repeats = 0u;
    rgbw_mailbox.heartbeat = 0u;
    rgbw_mailbox.error_flags = ERROR_NONE;
}

int main(void)
{
    uint32_t seen_sequence;
    uint32_t mode = MODE_OFF;
    uint32_t last_static_command = 0u;
    uint32_t next_state_at = 0u;
    uint32_t state_index = 0u;
    uint32_t completed = 0u;
    uint32_t state_count = 0u;
    uint32_t repeat_count = 0u;
    uint32_t dwell_cycles = 0u;

    clock_init();
    timing_init();
    gpio_init();
    all_off();
    mailbox_init();
    seen_sequence = rgbw_mailbox.command_sequence;

    while (1) {
        const uint32_t now = DWT_CYCCNT;
        const uint32_t command_sequence = rgbw_mailbox.command_sequence;
        rgbw_mailbox.heartbeat += 1u;

        if (command_sequence != seen_sequence) {
            const uint32_t requested_mode = rgbw_mailbox.mode;
            seen_sequence = command_sequence;
            rgbw_mailbox.error_flags = ERROR_NONE;
            rgbw_mailbox.current_state = 0u;
            rgbw_mailbox.completed_repeats = 0u;

            if (requested_mode == MODE_OFF) {
                all_off();
                mode = MODE_OFF;
                rgbw_mailbox.status = STATUS_IDLE;
            } else if (requested_mode == MODE_STATIC) {
                show(rgbw_mailbox.pixel0_rgbw, rgbw_mailbox.pixel1_rgbw);
                last_static_command = DWT_CYCCNT;
                mode = MODE_STATIC;
                rgbw_mailbox.status = STATUS_STATIC;
            } else if (requested_mode == MODE_SCAN) {
                state_count = rgbw_mailbox.state_count;
                repeat_count = rgbw_mailbox.repeat_count;
                if ((state_count == 0u) || (state_count > MAX_SCAN_STATES) ||
                    (repeat_count == 0u) || (repeat_count > 100u) ||
                    (rgbw_mailbox.dwell_us < 500u) ||
                    (rgbw_mailbox.dwell_us > 1000000u)) {
                    all_off();
                    mode = MODE_OFF;
                    rgbw_mailbox.status = STATUS_ERROR;
                    rgbw_mailbox.error_flags = ERROR_BAD_SCAN;
                } else {
                    dwell_cycles = rgbw_mailbox.dwell_us * (CPU_HZ / 1000000u);
                    state_index = 0u;
                    completed = 0u;
                    show(rgbw_mailbox.states[0][0], rgbw_mailbox.states[0][1]);
                    next_state_at = DWT_CYCCNT + dwell_cycles;
                    mode = MODE_SCAN;
                    rgbw_mailbox.status = STATUS_SCANNING;
                }
            } else {
                all_off();
                mode = MODE_OFF;
                rgbw_mailbox.status = STATUS_ERROR;
                rgbw_mailbox.error_flags = ERROR_BAD_MODE;
            }
            rgbw_mailbox.applied_sequence = command_sequence;
        }

        if ((mode == MODE_STATIC) &&
            ((uint32_t)(DWT_CYCCNT - last_static_command) >= STATIC_WATCHDOG)) {
            all_off();
            mode = MODE_OFF;
            rgbw_mailbox.status = STATUS_ERROR;
            rgbw_mailbox.error_flags = ERROR_WATCHDOG;
        }

        if ((mode == MODE_SCAN) && ((int32_t)(now - next_state_at) >= 0)) {
            state_index += 1u;
            if (state_index >= state_count) {
                state_index = 0u;
                completed += 1u;
                rgbw_mailbox.completed_repeats = completed;
                if (completed >= repeat_count) {
                    all_off();
                    mode = MODE_OFF;
                    rgbw_mailbox.status = STATUS_COMPLETE;
                    continue;
                }
            }
            show(rgbw_mailbox.states[state_index][0],
                 rgbw_mailbox.states[state_index][1]);
            rgbw_mailbox.current_state = state_index;
            next_state_at += dwell_cycles;
        }
    }
}
