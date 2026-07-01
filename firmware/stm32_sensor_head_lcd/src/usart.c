#include "usart.h"
#include <sys/stat.h>

UART_HandleTypeDef UART1_Handler;

void uart_init(u32 bound)
{
    UART1_Handler.Instance = USART1;
    UART1_Handler.Init.BaudRate = bound;
    UART1_Handler.Init.WordLength = UART_WORDLENGTH_8B;
    UART1_Handler.Init.StopBits = UART_STOPBITS_1;
    UART1_Handler.Init.Parity = UART_PARITY_NONE;
    UART1_Handler.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    UART1_Handler.Init.Mode = UART_MODE_TX_RX;
    UART1_Handler.Init.OverSampling = UART_OVERSAMPLING_16;
    UART1_Handler.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    UART1_Handler.Init.Prescaler = UART_PRESCALER_DIV1;
    UART1_Handler.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&UART1_Handler);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void serial_write(const char *s)
{
    if (!s) return;
    const char *p = s;
    while (*p) p++;
    HAL_UART_Transmit(&UART1_Handler, (uint8_t *)s, (uint16_t)(p - s), 1000);
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (len > 0) HAL_UART_Transmit(&UART1_Handler, (uint8_t *)ptr, (uint16_t)len, 1000);
    return len;
}

int fputc(int ch, FILE *f)
{
    (void)f;
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&UART1_Handler, &c, 1, 1000);
    return ch;
}

void *_sbrk(ptrdiff_t incr)
{
    extern char _end;
    extern char _estack;
    static char *heap_end;
    char *prev;
    if (heap_end == 0) heap_end = &_end;
    prev = heap_end;
    if (heap_end + incr > &_estack) return (void *)-1;
    heap_end += incr;
    return prev;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }

