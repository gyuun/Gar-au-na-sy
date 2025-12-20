#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "lcd.h"

// =========================
// (필요 시) LCD 해상도 설정
// =========================
#if defined(LCD_WIDTH) && defined(LCD_HEIGHT)
  #define LCD_SCREEN_W   LCD_WIDTH
  #define LCD_SCREEN_H   LCD_HEIGHT
#elif defined(LCD_W) && defined(LCD_H)
  #define LCD_SCREEN_W   LCD_W
  #define LCD_SCREEN_H   LCD_H
#elif defined(LCD_X_MAX) && defined(LCD_Y_MAX)
  #define LCD_SCREEN_W   LCD_X_MAX
  #define LCD_SCREEN_H   LCD_Y_MAX
#else
  #define LCD_SCREEN_W   240
  #define LCD_SCREEN_H   320
#endif

// 기본 폰트(대개 8x16)
#define FONT_W          8
#define FONT_H          16

// =========================
// 초음파 센서 핀 설정 (GPIOB)
// =========================
#define US_TRIG_PIN     GPIO_Pin_10
#define US_ECHO_PIN     GPIO_Pin_11
#define US_GPIO_PORT    GPIOB

#ifndef ORANGE
  #define ORANGE        0xFD20
#endif

#define MEASURE_PERIOD_US   60000
#define ECHO_TIMEOUT_US     30000

void Init(void);
void RccInit(void);
void GpioInit(void);
void TimerInit(void);

void Delay_us(uint32_t us);
uint32_t Get_Distance_cm(void);

static uint16_t BgColorFromDistance(uint32_t dist_cm);
static const char* StatusTextFromDistance(uint32_t dist_cm);
static void DrawCenteredStatus(const char* text, uint16_t textColor, uint16_t bgColor);

int main(void)
{
    uint32_t dist_cm = 0;
    uint16_t bg = BLUE;
    uint16_t prev_bg = 0xFFFF;

    Init();

    while (1)
    {
        // 1) 초음파 거리 측정
        dist_cm = Get_Distance_cm();

        // 타임아웃/비정상값 처리: 0이면 미측정으로 간주(멀리 있다고 보고 Normal로 처리)
        if (dist_cm == 0) dist_cm = 999;
        if (dist_cm > 999) dist_cm = 999;

        // 2) 배경색 결정
        bg = BgColorFromDistance(dist_cm);

        // 3) 배경색이 바뀐 경우에만 전체 갱신
        if (bg != prev_bg)
        {
            LCD_Clear(bg);
            prev_bg = bg;
        }

        // 4) 상태 텍스트를 정중앙에 흰색으로 출력
        DrawCenteredStatus(StatusTextFromDistance(dist_cm), WHITE, bg);

        // 5) 측정 주기 딜레이
        Delay_us(MEASURE_PERIOD_US);
    }
}

// 파란색(11~16), 주황색(6~10), 빨간색(~5)
// (요구사항 외: 11 이상은 파란색으로 처리)
static uint16_t BgColorFromDistance(uint32_t dist_cm)
{
    if (dist_cm <= 5)       return RED;
    else if (dist_cm <= 10) return ORANGE;
    else                    return BLUE;
}

// 색상에 대응하는 상태 텍스트
// Blue -> Normal, Orange -> Caution, Red -> Warning
static const char* StatusTextFromDistance(uint32_t dist_cm)
{
    if (dist_cm <= 5)       return "Warning";
    else if (dist_cm <= 10) return "Caution";
    else                    return "Normal";
}

// 텍스트를 화면 정중앙에 출력
static void DrawCenteredStatus(const char* text, uint16_t textColor, uint16_t bgColor)
{
    // 문자열 길이 계산 (표준 라이브러리(strlen) 없이 직접)
    uint16_t len = 0;
    while (text[len] != '\0') len++;

    uint16_t x0 = (LCD_SCREEN_W - (len * FONT_W)) / 2;
    uint16_t y0 = (LCD_SCREEN_H - FONT_H) / 2;

    // 같은 버킷 내에서 반복 출력 시 잔상 방지:
    // 배경색으로 텍스트 영역을 한번 덮은 뒤 다시 출력
    // (LCD 라이브러리에 FillRect가 없을 수 있어 문자열 공백 덮기로 처리)
    {
        // 최대 글자수 10 정도로 가정해 공백 덮기
        // "Warning"(7), "Caution"(7), "Normal"(6)
        LCD_ShowString(x0, y0, "          ", bgColor, bgColor);
    }

    LCD_ShowString(x0, y0, (char*)text, textColor, bgColor);
}

void Init(void)
{
    SystemInit();
    RccInit();
    GpioInit();
    TimerInit();

    LCD_Init();
}

void RccInit(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
}

void GpioInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // Trig (PB10): Output Push-Pull
    GPIO_InitStructure.GPIO_Pin = US_TRIG_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(US_GPIO_PORT, &GPIO_InitStructure);

    // Echo (PB11): Input Floating
    GPIO_InitStructure.GPIO_Pin = US_ECHO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(US_GPIO_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(US_GPIO_PORT, US_TRIG_PIN);
}

void TimerInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    // 72MHz 기준: Prescaler=71 => 1MHz (1us tick)
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_Cmd(TIM2, ENABLE);
}

void Delay_us(uint32_t us)
{
    TIM_SetCounter(TIM2, 0);
    while (TIM_GetCounter(TIM2) < us);
}

// 반환: cm (타임아웃/비정상 시 0)
uint32_t Get_Distance_cm(void)
{
    uint32_t t_us;

    // Trig: 10us pulse
    GPIO_ResetBits(US_GPIO_PORT, US_TRIG_PIN);
    Delay_us(2);

    GPIO_SetBits(US_GPIO_PORT, US_TRIG_PIN);
    Delay_us(10);
    GPIO_ResetBits(US_GPIO_PORT, US_TRIG_PIN);

    // Echo HIGH 대기(타임아웃)
    TIM_SetCounter(TIM2, 0);
    while (GPIO_ReadInputDataBit(US_GPIO_PORT, US_ECHO_PIN) == RESET)
    {
        if (TIM_GetCounter(TIM2) > ECHO_TIMEOUT_US)
            return 0;
    }

    // Echo HIGH 폭 측정(타임아웃)
    TIM_SetCounter(TIM2, 0);
    while (GPIO_ReadInputDataBit(US_GPIO_PORT, US_ECHO_PIN) == SET)
    {
        if (TIM_GetCounter(TIM2) > ECHO_TIMEOUT_US)
            break;
    }

    t_us = TIM_GetCounter(TIM2);

    // cm ≈ t_us / 58
    return (t_us / 58);
}
