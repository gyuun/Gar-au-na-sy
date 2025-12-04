#include "stm32f10x_adc.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "lcd.h"
#include "touch.h"
#include "math.h"

// LCD UI 관련 정의
#define LCD_TEAM_NAME_X 20
#define LCD_TEAM_NAME_Y 50
#define LCD_STATUS_X 20
#define LCD_STATUS_Y 70

// 함수 프로토타입 선언
void Init(void);
void RccInit(void);
void GpioInit(void);
void TIM_Configure(void);
void NvicInit(void);
void ledToggle(int num);
void setServoPulse(uint16_t pulse); // 특정 펄스폭으로 이동하는 함수

const int color[12] = {WHITE,CYAN,BLUE,RED,MAGENTA,LGRAY,GREEN,YELLOW,BROWN,BRRED,GRAY};

// timer counter
int timer_counter = 0;
// led on/off
char ledOn = 0;

// Motor PWM 구조체
TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
TIM_OCInitTypeDef  TIM_OCInitStructure;

// 버튼 상태 관리를 위한 변수
int btnPrevPressed = 0; // 이전 버튼 상태
int motorState = 0;     // 0: 각도 A, 1: 각도 B (토글용)

int main(){
    Init();

    ledOn = 1; // LED 기본 켜짐
    LCD_Clear(WHITE);

    // LCD 초기 화면
    LCD_ShowString(LCD_TEAM_NAME_X, LCD_TEAM_NAME_Y, "SERVO CTRL", BLUE, WHITE);
    LCD_ShowString(LCD_STATUS_X, LCD_STATUS_Y, "BTN: PC4", RED, WHITE);
    
    // 초기 모터 위치 설정 (0도, 펄스 700us)
    setServoPulse(700);

    while(1){
        // GPIO PC4 입력 읽기 (Input Pull-up 가정: 누르면 0, 떼면 1)
        // 만약 버튼 회로가 Active-High라면 Bit_SET으로 변경해야 함
        int btnPressed = (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET);

        // 버튼이 눌리는 순간 (Rising Edge of Press action) 감지
        if (btnPressed && !btnPrevPressed) {
            ledToggle(1); // 버튼 입력 확인용 LED 토글

            if (motorState == 0) {
                // 특정 각도로 이동 (예: 90도 근처, 펄스 1500us)
                setServoPulse(1500); 
                LCD_ShowString(LCD_STATUS_X, LCD_STATUS_Y + 20, "Angle: 90 ", BLACK, WHITE);
                motorState = 1;
            } else {
                // 원위치로 이동 (예: 0도, 펄스 700us)
                setServoPulse(700);
                LCD_ShowString(LCD_STATUS_X, LCD_STATUS_Y + 20, "Angle: 0  ", BLACK, WHITE);
                motorState = 0;
            }
        }
        btnPrevPressed = btnPressed;
        
        // 간단한 디바운싱 지연 (필요 시 조절)
        for(int i=0; i<10000; i++);
    }
}

void Init(void) {
    SystemInit();
    RccInit();
    GpioInit();
    TIM_Configure();
    NvicInit();

    LCD_Init();
    // 터치 관련 설정은 필요 없다면 주석 처리 가능
    // Touch_Configuration(); 
    
    // LEDs 초기화
    GPIO_SetBits(GPIOD, GPIO_Pin_2);
    GPIO_SetBits(GPIOD, GPIO_Pin_3);
}

void RccInit(void) {
    // GPIOC 클럭 추가 (PC4 사용을 위해)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD | 
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // LED toggle timer
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // PWM timer
}

void GpioInit(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // LED 1 (PD2), LED 2 (PD3)
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // PWM motor (TIM3_CH3 -> PB0)
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // [추가됨] Button 1 (PC4) 설정
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU; // Input Pull-up (누르면 Low)
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void TIM_Configure(void)
{
    // TIM2: LED 점멸용 (기존 유지)
    TIM_TimeBaseInitTypeDef TIM2_InitStructure;

    TIM2_InitStructure.TIM_Period        = 10000;
    TIM2_InitStructure.TIM_Prescaler     = 7200;
    TIM2_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM2_InitStructure.TIM_CounterMode   = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM2_InitStructure);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // TIM3: 모터 PWM용 (기존 유지)
    TIM_TimeBaseInitTypeDef TIM3_InitStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure_local;

    uint16_t prescale = (uint16_t)(SystemCoreClock / 1000000); // 1MHz tick

    TIM3_InitStructure.TIM_Period        = 20000;              // 20ms 주기
    TIM3_InitStructure.TIM_Prescaler     = prescale;
    TIM3_InitStructure.TIM_ClockDivision = 0;
    TIM3_InitStructure.TIM_CounterMode   = TIM_CounterMode_Down;

    TIM_OCInitStructure_local.TIM_OCMode       = TIM_OCMode_PWM1;
    TIM_OCInitStructure_local.TIM_OCPolarity   = TIM_OCPolarity_High;
    TIM_OCInitStructure_local.TIM_OutputState  = TIM_OutputState_Enable;
    TIM_OCInitStructure_local.TIM_Pulse        = 700; // 초기 위치

    TIM_OC3Init(TIM3, &TIM_OCInitStructure_local);

    TIM_TimeBaseInit(TIM3, &TIM3_InitStructure);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void NvicInit(void){
    NVIC_InitTypeDef NVIC_InitStructure;

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void ledToggle(int num) {
    uint16_t pin;
    switch (num) {
        case 1: pin = GPIO_Pin_2; break;
        case 2: pin = GPIO_Pin_3; break;
        default: return;
    }
    if (GPIO_ReadOutputDataBit(GPIOD, pin) == Bit_RESET) GPIO_SetBits(GPIOD, pin);
    else                                                GPIO_ResetBits(GPIOD, pin);
}

// [수정됨] 특정 펄스 값으로 서보모터를 즉시 이동시키는 함수
void setServoPulse(uint16_t pulse){
    TIM_OCInitStructure.TIM_OCMode       = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity   = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState  = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse        = pulse; 

    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
}

void TIM2_IRQHandler(void) {
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        timer_counter++;
        
        // [삭제됨] moveMotor(); 
        // 타이머 인터럽트 내에서의 자동 모터 이동 코드를 삭제했습니다.
        // 이제 모터 제어는 main 함수의 버튼 입력 부분에서 담당합니다.

        // LED toggle logic (살아있음을 표시하기 위해 유지)
        if (ledOn) {
            // 2초에 한 번 정도 깜빡임
             if (timer_counter % 2 == 0) ledToggle(2);
        }

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}