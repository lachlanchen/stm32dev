/* Finite, monitored reference acquisition. Existing source files stay intact. */
#define main preserved_short_qyh_main
#include "qyh1123_mcp4728_black_test_main.c"
#undef main

volatile uint32_t sef_magic = 0x53454631u, sef_ready, sef_request, sef_cancel;
volatile uint32_t sef_busy, sef_fault, sef_completed, sef_elapsed_ms;
volatile uint32_t sef_duty, sef_lcd_mv, sef_cool_remaining_ms;
volatile uint32_t sef_ack_failures, sef_heartbeat;
volatile uint32_t sef_duration_ms = 2000u, sef_points = 129u;
volatile uint32_t sef_lamp_table[257], sef_lcd_table[257];
volatile uint32_t ref_present_mask, ref_monitor_channel = 2u, ref_cap;
volatile uint32_t ref_bus_mV[4];
volatile int32_t ref_current_mA[4];
volatile uint32_t ref_trace_count, ref_late_ms, ref_trace[128][8];
static uint8_t ref_dac;

static bool ref_watchdog_start(void) {
    IWDG1->KR=0xCCCCu; IWDG1->KR=0x5555u; IWDG1->PR=6u; IWDG1->RLR=249u;
    uint32_t started=HAL_GetTick();
    while(IWDG1->SR!=0u) { if((uint32_t)(HAL_GetTick()-started)>100u) return false; }
    IWDG1->KR=0xAAAAu; return true;
}

static void ref_delay(void) { delay_us(2); }
static void ref_scl(bool high) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void ref_sda(bool high) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void ref_start(void) {
    ref_sda(true); ref_scl(true); ref_delay(); ref_sda(false); ref_delay(); ref_scl(false);
}
static void ref_stop(void) {
    ref_sda(false); ref_delay(); ref_scl(true); ref_delay(); ref_sda(true); ref_delay();
}
static bool ref_write(uint8_t value) {
    for (uint32_t i=0;i<8;i++) {
        ref_sda((value & 128u)!=0); ref_delay(); ref_scl(true); ref_delay(); ref_scl(false); value<<=1;
    }
    ref_sda(true); ref_delay(); ref_scl(true); ref_delay();
    bool ok=HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)==GPIO_PIN_RESET;
    ref_scl(false); ref_delay(); return ok;
}
static uint8_t ref_read(bool ack) {
    uint8_t value=0; ref_sda(true);
    for (uint32_t i=0;i<8;i++) {
        ref_delay(); ref_scl(true); ref_delay(); value=(uint8_t)((value<<1)|(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)==GPIO_PIN_SET)); ref_scl(false);
    }
    ref_sda(!ack); ref_delay(); ref_scl(true); ref_delay(); ref_scl(false); ref_sda(true); return value;
}
static bool ref_reg_read(uint8_t address,uint8_t reg,uint16_t *value) {
    ref_start(); bool ok=ref_write(address<<1); ok=ref_write(reg)&&ok;
    ref_start(); ok=ref_write((address<<1)|1u)&&ok;
    if(ok) { uint8_t h=ref_read(true),l=ref_read(false); *value=((uint16_t)h<<8)|l; }
    ref_stop(); return ok;
}
static bool ref_reg_write(uint8_t address,uint8_t reg,uint16_t value) {
    ref_start(); bool ok=ref_write(address<<1); ok=ref_write(reg)&&ok;
    ok=ref_write(value>>8)&&ok; ok=ref_write(value&255u)&&ok; ref_stop(); return ok;
}
static bool ref_monitor(uint32_t channel) {
    uint16_t bus=0,shunt=0;
    if(channel>3 || !ref_reg_read(0x40u+channel,2,&bus) || !ref_reg_read(0x40u+channel,1,&shunt)) return false;
    ref_bus_mV[channel]=(bus>>3)*4u;
    /* HAT 0.1 ohm shunt; preserve signed current. Not a direct RMS measurement. */
    ref_current_mA[channel]=((int32_t)(int16_t)shunt*10)/100;
    return true;
}
static bool ref_supply_ok(void) {
    uint32_t ch=ref_monitor_channel;
    if(ch>3 || !(ref_present_mask&(1u<<ch)) || !ref_monitor(ch)) { sef_fault=4; return false; }
    uint32_t supply=ref_bus_mV[ch];
    if(supply<6000u || supply>14000u) { sef_fault=5; return false; }
    int32_t current=ref_current_mA[ch];
    if(current>2000 || current< -2000) { sef_fault=7; return false; }
    /* A low off-state bus reading must never authorize increased duty.
     * User confirms a nominal 12 V supply: use at least 12.5 V for the cap.
     * This is a conservative command bound, not measured lamp RMS voltage. */
    if(current>0) supply+=(uint32_t)current/10u;
    if(supply<12500u) supply=12500u;
    ref_cap=(uint32_t)((36000000ull*65535ull)/((uint64_t)supply*supply));
    if(ref_cap>16383u) ref_cap=16383u;
    return true;
}
static void ref_pwm_init(void) {
    GPIO_InitTypeDef gpio={0}; __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_TIM5_CLK_ENABLE();
    gpio.Pin=GPIO_PIN_2; gpio.Mode=GPIO_MODE_AF_PP; gpio.Pull=GPIO_PULLDOWN;
    gpio.Speed=GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate=GPIO_AF2_TIM5; HAL_GPIO_Init(GPIOA,&gpio);
    TIM5->CR1=0; TIM5->PSC=6; TIM5->ARR=65535; TIM5->CCR3=0;
    TIM5->CCMR2=(TIM5->CCMR2&~(TIM_CCMR2_OC3M|TIM_CCMR2_CC3S))|TIM_CCMR2_OC3M_1|TIM_CCMR2_OC3M_2|TIM_CCMR2_OC3PE;
    TIM5->CCER=(TIM5->CCER&~(TIM_CCER_CC3P|TIM_CCER_CC3NP))|TIM_CCER_CC3E;
    TIM5->EGR=TIM_EGR_UG; TIM5->CR1=TIM_CR1_ARPE|TIM_CR1_CEN;
}
static void ref_pwm(uint32_t value) {
    if(value>ref_cap) value=ref_cap;
    TIM5->CCR3=value; sef_duty=value;
}
static bool ref_lcd(uint32_t amplitude,bool positive) {
    if(amplitude>3000u) amplitude=3000u;
    int32_t diff=positive?(int32_t)amplitude:-(int32_t)amplitude;
    bool ok=mcp4728_set_pair(ref_dac,LCD_MV_TO_CODE((3000+diff)/2),LCD_MV_TO_CODE((3000-diff)/2));
    if(!ok) { ++sef_ack_failures; sef_fault=2; }
    sef_lcd_mv=amplitude; return ok;
}
static uint32_t ref_interp(volatile uint32_t *table,uint32_t elapsed) {
    uint32_t scale=elapsed*(sef_points-1u),index=scale/sef_duration_ms,remainder=scale%sef_duration_ms;
    if(index>=sef_points-1u) return table[sef_points-1u];
    int64_t value=(int64_t)table[index]+((int64_t)((int32_t)table[index+1]-(int32_t)table[index])*remainder)/sef_duration_ms;
    return value<0?0:(uint32_t)value;
}
static void ref_stop_all(void) {
    ref_pwm(0); if(ref_dac) ref_lcd(0,true); sef_busy=0; sef_request=0; sef_cancel=0;
}
int main(void) {
    SCB_EnableICache(); HAL_Init(); Stm32_Clock_Init(160,5,2,4); delay_init(400);
    optical_outputs_force_safe(); dwt_timer_init(); i2c_bus_init(); ref_pwm_init();
    if(sef_magic!=0x53454631u) sef_fault=8;
    ref_dac=mcp4728_find(); if(!ref_dac || !ref_lcd(0,true)) sef_fault=1;
    for(uint32_t ch=0;ch<4;ch++) {
        if(ref_reg_write(0x40u+ch,0,0x3FFFu) && ref_monitor(ch)) ref_present_mask|=1u<<ch;
    }
    if(!ref_watchdog_start()) sef_fault=3;
    sef_ready=1;
    uint32_t started=0,cooling=0,next_carrier=0,next_sample=0,half=0,amplitude=0;
    while(1) {
        IWDG1->KR=0xAAAAu; ++sef_heartbeat; uint32_t now=HAL_GetTick();
        sef_cool_remaining_ms=(int32_t)(cooling-now)>0?cooling-now:0;
        if(sef_cancel || sef_fault) { bool was_busy=sef_busy!=0; ref_stop_all(); if(was_busy) cooling=now+10000u; }
        if(sef_request && !sef_busy) {
            sef_request=0;
            if(!sef_fault && !sef_cool_remaining_ms) {
                if(sef_duration_ms<500u || sef_duration_ms>2000u || sef_points<2u || sef_points>257u) sef_fault=6;
                else if(ref_supply_ok()) {
                    sef_busy=1; sef_elapsed_ms=0; ref_trace_count=0; ref_late_ms=0;
                    started=HAL_GetTick(); next_carrier=started; next_sample=started; half=0;
                }
            }
        }
        if(sef_busy) {
            now=HAL_GetTick(); uint32_t elapsed=now-started; sef_elapsed_ms=elapsed;
            if(elapsed>=sef_duration_ms || elapsed>=2000u) {
                ref_stop_all(); ++sef_completed; cooling=now+10000u;
            } else {
                ref_pwm(ref_interp(sef_lamp_table,elapsed));
                if((int32_t)(now-next_carrier)>=0) {
                    uint32_t late=now-next_carrier; if(late>ref_late_ms) ref_late_ms=late;
                    if((half&1u)==0) amplitude=ref_interp(sef_lcd_table,elapsed);
                    if(!ref_lcd(amplitude,(half&1u)==0)) ref_pwm(0);
                    ++half; next_carrier+=5u;
                    if((int32_t)(HAL_GetTick()-next_carrier)>=0) { sef_fault=2; ref_pwm(0); }
                    if((int32_t)(now-next_sample)>=0 && !sef_fault) {
                        if(!ref_supply_ok()) ref_pwm(0);
                        if(ref_trace_count<128u) {
                            uint32_t n=ref_trace_count; ref_trace[n][0]=elapsed; ref_trace[n][1]=sef_duty;
                            ref_trace[n][2]=sef_lcd_mv; ref_trace[n][3]=ref_bus_mV[ref_monitor_channel];
                            ref_trace[n][4]=(uint32_t)ref_current_mA[ref_monitor_channel]; ref_trace[n][5]=ref_late_ms;
                            ref_trace[n][6]=sef_ack_failures; ref_trace[n][7]=sef_fault; ref_trace_count=n+1u;
                        }
                        next_sample+=20u;
                    }
                }
            }
        }
        delay_us(20);
    }
}
