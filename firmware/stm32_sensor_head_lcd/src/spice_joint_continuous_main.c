/* DCP2: finite joint trajectories, no sustained source hold.
 * Separate image; DCP1 source and backups remain unchanged.
 * Maximum 28 s, nominal source <=6 V, mandatory 5 s cooling.
 */
#define main preserved_qyh_main
#include "qyh1123_mcp4728_black_test_main.c"
#undef main

volatile uint32_t cap_magic=0x44435032u, cap_ready, cap_request, cap_cancel;
volatile uint32_t cap_busy, cap_fault, cap_completed, cap_elapsed_ms;
volatile uint32_t cap_duty, cap_lamp_mv, cap_lcd1_mv, cap_lcd2_mv;
volatile uint32_t cap_cool_remaining_ms, cap_ack_failures, cap_updates, cap_late_halves;
volatile uint32_t cap_heartbeat, cap_high_spent_ms, cap_high_allowed;
volatile uint32_t cap_duration_ms=4000u, cap_points=2u;
volatile uint32_t cap_hold_enable, cap_hold1_mv, cap_hold2_mv;
volatile uint32_t cap_lamp_table[257], cap_lcd1_table[257], cap_lcd2_table[257];
volatile uint32_t cap_dac_address, cap_dac_count;

static void capture_pwm_init(void)
{
    GPIO_InitTypeDef g={0};
    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1=0; TIM5->PSC=6; TIM5->ARR=65535; TIM5->CCR3=0;
    TIM5->CCMR2=(TIM5->CCMR2&~(TIM_CCMR2_OC3M|TIM_CCMR2_CC3S))|
        TIM_CCMR2_OC3M_1|TIM_CCMR2_OC3M_2|TIM_CCMR2_OC3PE;
    TIM5->CCER=(TIM5->CCER&~(TIM_CCER_CC3P|TIM_CCER_CC3NP))|TIM_CCER_CC3E;
    TIM5->EGR=TIM_EGR_UG;
    g.Pin=GPIO_PIN_2; g.Mode=GPIO_MODE_AF_PP; g.Pull=GPIO_PULLDOWN;
    g.Speed=GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate=GPIO_AF2_TIM5;
    HAL_GPIO_Init(GPIOA,&g); TIM5->CR1=TIM_CR1_ARPE|TIM_CR1_CEN;
}

static void capture_pwm(uint32_t mv)
{
    if(mv>6000u) mv=6000u;
    cap_lamp_mv=mv;
    cap_duty=(uint32_t)(((uint64_t)mv*mv*65536u)/(12500ull*12500ull));
    TIM5->CCR3=cap_duty;
}

static bool capture_lcd(uint32_t one,uint32_t two,bool positive)
{
    if(one>3000u) one=3000u;
    if(two>3000u) two=3000u;
    uint16_t code[4];
    code[0]=(LCD_CODE_HIGH+LCD_MV_TO_CODE(one))/2u;
    code[1]=LCD_CODE_HIGH-code[0];
    code[2]=(LCD_CODE_HIGH+LCD_MV_TO_CODE(two))/2u;
    code[3]=LCD_CODE_HIGH-code[2];
    if(!positive) {
        uint16_t t=code[0]; code[0]=code[1]; code[1]=t;
        t=code[2]; code[2]=code[3]; code[3]=t;
    }
    bool ok=true;
    for(uint8_t ch=0;ch<4;ch++)
        ok=mcp4728_stage_channel((uint8_t)cap_dac_address,ch,code[ch])&&ok;
    if(ok) ok=mcp4728_commit_all();
    if(!ok) { ++cap_ack_failures; cap_fault=2; capture_pwm(0); }
    else { cap_lcd1_mv=one; cap_lcd2_mv=two; ++cap_updates; }
    return ok;
}

static uint32_t capture_value(volatile uint32_t *table,uint32_t elapsed)
{
    uint32_t scaled=elapsed*(cap_points-1u);
    uint32_t i=scaled/cap_duration_ms, f=scaled%cap_duration_ms;
    if(i>=cap_points-1u) return table[cap_points-1u];
    return (uint32_t)(((uint64_t)table[i]*(cap_duration_ms-f)+
                       (uint64_t)table[i+1u]*f)/cap_duration_ms);
}

int main(void)
{
    SCB_EnableICache(); HAL_Init(); Stm32_Clock_Init(160,5,2,4);
    delay_init(400); optical_outputs_force_safe(); capture_pwm_init(); i2c_bus_init();
    if(cap_magic!=0x44435032u) { while(1) { capture_pwm(0); } }
    for(uint8_t a=0x60;a<=0x67;a++) if(i2c_present(a)) {
        if(!cap_dac_count) cap_dac_address=a;
        ++cap_dac_count;
    }
    if(!cap_dac_count || !capture_lcd(0,0,true)) cap_fault=1;
    cap_ready=1;
    IWDG1->KR=0x5555; IWDG1->PR=6; IWDG1->RLR=250;
    IWDG1->KR=0xAAAA; IWDG1->KR=0xCCCC;
    uint32_t start=0, cool_until=0, half=0, next_half=HAL_GetTick();
    uint32_t one=0,two=0;
    while(1) {
        IWDG1->KR=0xAAAA; ++cap_heartbeat;
        uint32_t now=HAL_GetTick();
        cap_cool_remaining_ms=(int32_t)(cool_until-now)>0?cool_until-now:0;
        if(cap_cancel || cap_fault) {
            cap_cancel=0; capture_pwm(0);
            if(cap_busy) cool_until=now+5000u;
            cap_busy=0;
        }
        if(cap_request) {
            cap_request=0;
            if(!cap_fault && !cap_busy && !cap_cool_remaining_ms) {
                bool valid=cap_points>=2u && cap_points<=257u &&
                    cap_duration_ms>=200u && cap_duration_ms<=28000u;
                if(valid) for(uint32_t i=0;i<cap_points;i++)
                    if(cap_lamp_table[i]>6000u || cap_lcd1_table[i]>3000u ||
                       cap_lcd2_table[i]>3000u) valid=false;
                if(!valid) cap_fault=3;
                else {start=now;cap_busy=1;cap_elapsed_ms=0;half=0;next_half=now;}
            }
        }
        if(cap_busy) {
            cap_elapsed_ms=now-start;
            if(cap_elapsed_ms>=cap_duration_ms) {
                capture_pwm(0);cap_busy=0;++cap_completed;cool_until=now+5000u;
            } else capture_pwm(capture_value(cap_lamp_table,cap_elapsed_ms));
        } else capture_pwm(0);
        if((int32_t)(now-next_half)>=0) {
            if(now-next_half>2u) ++cap_late_halves;
            if((half&1u)==0u) {
                if(cap_busy) {
                    one=capture_value(cap_lcd1_table,cap_elapsed_ms);
                    two=capture_value(cap_lcd2_table,cap_elapsed_ms);
                } else {
                    one=cap_hold_enable?cap_hold1_mv:0;
                    two=cap_hold_enable?cap_hold2_mv:0;
                }
                if(cap_fault) one=two=0;
            }
            if(cap_dac_count) (void)capture_lcd(one,two,(half&1u)==0u);
            ++half;next_half+=5u;
            if((int32_t)(HAL_GetTick()-next_half)>=0) next_half=HAL_GetTick()+1u;
        }
    }
}
