#include "sys.h"
#include "delay.h"
#include "usart.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define W25Q_CMD_READ_JEDEC_ID       0x9Fu
#define W25Q_CMD_READ_STATUS_1       0x05u
#define W25Q_CMD_READ_STATUS_2       0x35u
#define W25Q_CMD_READ_STATUS_3       0x15u
#define W25Q_CMD_READ_SFDP           0x5Au
#define W25Q_CMD_READ_UNIQUE_ID      0x4Bu
#define W25Q_CMD_READ_DATA           0x03u

#define W25Q64_JEDEC_ID              0x00EF4017u
#define QSPI_ID_CHECKS               64u
#define QSPI_TIMEOUT_MS              100u

#define QSPI_FAIL_INIT               (1u << 0)
#define QSPI_FAIL_ID_TRANSFER        (1u << 1)
#define QSPI_FAIL_ID                 (1u << 2)
#define QSPI_FAIL_STABILITY          (1u << 3)

QSPI_HandleTypeDef QSPI_Handler;

/* Stable symbols for non-invasive OpenOCD/ST-Link inspection. */
volatile uint32_t qspi_diag_magic = 0x51535049u;
volatile uint32_t qspi_diag_result = 0u;
volatile uint32_t qspi_diag_fail_mask = 0xFFFFFFFFu;
volatile uint32_t qspi_diag_jedec_id = 0u;
volatile uint32_t qspi_diag_status_1_3 = 0u;
volatile uint32_t qspi_diag_sfdp_signature = 0u;
volatile uint32_t qspi_diag_sfdp_valid = 0u;
volatile uint32_t qspi_diag_unique_id_lo = 0u;
volatile uint32_t qspi_diag_unique_id_hi = 0u;
volatile uint32_t qspi_diag_data_0_3 = 0u;
volatile uint32_t qspi_diag_expected_reads = 0u;
volatile uint32_t qspi_diag_consistent_reads = 0u;
volatile uint32_t qspi_diag_aux_read_errors = 0u;
volatile uint32_t qspi_diag_cycles = 0u;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static uint32_t pack_u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void lamps_force_off(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
    GPIO_InitTypeDef gpio = {0};

    if (hqspi->Instance != QUADSPI) return;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_QSPI_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Pin = GPIO_PIN_2;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_6;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);
}

static HAL_StatusTypeDef qspi_init_read_only(void)
{
    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();

    memset(&QSPI_Handler, 0, sizeof(QSPI_Handler));
    QSPI_Handler.Instance = QUADSPI;
    QSPI_Handler.Init.ClockPrescaler = 9u; /* 200 MHz / 10 = 20 MHz. */
    QSPI_Handler.Init.FifoThreshold = 4u;
    QSPI_Handler.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    QSPI_Handler.Init.FlashSize = 22u; /* 2^(22 + 1) = 8 MiB. */
    QSPI_Handler.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_5_CYCLE;
    QSPI_Handler.Init.ClockMode = QSPI_CLOCK_MODE_3;
    QSPI_Handler.Init.FlashID = QSPI_FLASH_ID_1;
    QSPI_Handler.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    return HAL_QSPI_Init(&QSPI_Handler);
}

static HAL_StatusTypeDef qspi_read(uint8_t instruction,
                                   uint32_t address,
                                   uint32_t address_mode,
                                   uint32_t address_size,
                                   uint32_t dummy_cycles,
                                   uint8_t *data,
                                   uint32_t length)
{
    QSPI_CommandTypeDef command = {0};
    HAL_StatusTypeDef status;

    command.Instruction = instruction;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Address = address;
    command.AddressMode = address_mode;
    command.AddressSize = address_size;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.DummyCycles = dummy_cycles;
    command.NbData = length;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    status = HAL_QSPI_Command(&QSPI_Handler, &command, QSPI_TIMEOUT_MS);
    if (status != HAL_OK) return status;
    return HAL_QSPI_Receive(&QSPI_Handler, data, QSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef read_no_address(uint8_t instruction,
                                         uint8_t *data,
                                         uint32_t length)
{
    return qspi_read(instruction, 0u, QSPI_ADDRESS_NONE,
                     QSPI_ADDRESS_24_BITS, 0u, data, length);
}

static uint32_t run_read_only_check(void)
{
    uint8_t id[3] = {0};
    uint8_t baseline[3] = {0};
    uint8_t status[3] = {0};
    uint8_t sfdp[8] = {0};
    uint8_t unique_id[8] = {0};
    uint8_t data_sample[4] = {0};
    uint32_t fail_mask = 0u;
    uint32_t expected = 0u;
    uint32_t consistent = 0u;
    uint32_t aux_errors = 0u;
    bool baseline_valid = false;

    for (uint32_t check = 0; check < QSPI_ID_CHECKS; ++check) {
        if (read_no_address(W25Q_CMD_READ_JEDEC_ID, id, sizeof(id)) != HAL_OK) {
            fail_mask |= QSPI_FAIL_ID_TRANSFER;
            continue;
        }
        if (!baseline_valid) {
            memcpy(baseline, id, sizeof(id));
            baseline_valid = true;
        }
        if (memcmp(id, baseline, sizeof(id)) == 0) consistent++;
        if (id[0] == 0xEFu && id[1] == 0x40u && id[2] == 0x17u) expected++;
    }

    qspi_diag_jedec_id = ((uint32_t)id[0] << 16) |
                         ((uint32_t)id[1] << 8) |
                         (uint32_t)id[2];
    qspi_diag_expected_reads = expected;
    qspi_diag_consistent_reads = consistent;

    if (expected != QSPI_ID_CHECKS) fail_mask |= QSPI_FAIL_ID;
    if (consistent != QSPI_ID_CHECKS) fail_mask |= QSPI_FAIL_STABILITY;

    if (read_no_address(W25Q_CMD_READ_STATUS_1, &status[0], 1u) != HAL_OK) aux_errors |= 1u << 0;
    if (read_no_address(W25Q_CMD_READ_STATUS_2, &status[1], 1u) != HAL_OK) aux_errors |= 1u << 1;
    if (read_no_address(W25Q_CMD_READ_STATUS_3, &status[2], 1u) != HAL_OK) aux_errors |= 1u << 2;
    qspi_diag_status_1_3 = (uint32_t)status[0] |
                           ((uint32_t)status[1] << 8) |
                           ((uint32_t)status[2] << 16);

    if (qspi_read(W25Q_CMD_READ_SFDP, 0u, QSPI_ADDRESS_1_LINE,
                  QSPI_ADDRESS_24_BITS, 8u, sfdp, sizeof(sfdp)) != HAL_OK) {
        aux_errors |= 1u << 3;
    }
    qspi_diag_sfdp_signature = pack_u32_le(sfdp);
    qspi_diag_sfdp_valid = (memcmp(sfdp, "SFDP", 4u) == 0) ? 1u : 0u;

    /* 0x4B needs four dummy bytes. A 24-bit zero address plus 8 dummy clocks
       produces the required 32 clocks without changing flash state. */
    if (qspi_read(W25Q_CMD_READ_UNIQUE_ID, 0u, QSPI_ADDRESS_1_LINE,
                  QSPI_ADDRESS_24_BITS, 8u, unique_id, sizeof(unique_id)) != HAL_OK) {
        aux_errors |= 1u << 4;
    }
    qspi_diag_unique_id_lo = pack_u32_le(unique_id);
    qspi_diag_unique_id_hi = pack_u32_le(unique_id + 4);

    if (qspi_read(W25Q_CMD_READ_DATA, 0u, QSPI_ADDRESS_1_LINE,
                  QSPI_ADDRESS_24_BITS, 0u, data_sample, sizeof(data_sample)) != HAL_OK) {
        aux_errors |= 1u << 5;
    }
    qspi_diag_data_0_3 = pack_u32_le(data_sample);
    qspi_diag_aux_read_errors = aux_errors;
    return fail_mask;
}

int main(void)
{
    HAL_StatusTypeDef init_status;

    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);
    uart_init(115200);
    lamps_force_off();
    qspi_diag_magic = 0x51535049u;

    init_status = qspi_init_read_only();
    printf("# W25Q64 read-only QSPI diagnostic; no write, program, or erase\r\n");

    while (1) {
        uint32_t fail_mask;

        if (init_status != HAL_OK) fail_mask = QSPI_FAIL_INIT;
        else fail_mask = run_read_only_check();

        qspi_diag_fail_mask = fail_mask;
        qspi_diag_result = (fail_mask == 0u) ? 1u : 0u;
        qspi_diag_cycles++;
        __DSB();

        printf("QSPI_DIAG,cycle=%lu,result=%s,jedec=%06lX,expected=%lu/%u,"
               "stable=%lu/%u,status=%06lX,sfdp=%08lX,sfdp_ok=%lu,"
               "uid=%08lX%08lX,data0=%08lX,aux=%08lX,fail=%08lX\r\n",
               (unsigned long)qspi_diag_cycles,
               fail_mask == 0u ? "PASS" : "FAIL",
               (unsigned long)qspi_diag_jedec_id,
               (unsigned long)qspi_diag_expected_reads, QSPI_ID_CHECKS,
               (unsigned long)qspi_diag_consistent_reads, QSPI_ID_CHECKS,
               (unsigned long)qspi_diag_status_1_3,
               (unsigned long)qspi_diag_sfdp_signature,
               (unsigned long)qspi_diag_sfdp_valid,
               (unsigned long)qspi_diag_unique_id_hi,
               (unsigned long)qspi_diag_unique_id_lo,
               (unsigned long)qspi_diag_data_0_3,
               (unsigned long)qspi_diag_aux_read_errors,
               (unsigned long)fail_mask);

        HAL_Delay(500u);
    }
}
