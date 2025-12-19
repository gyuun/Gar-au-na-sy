#include "stm32f10x.h"

// 레지스터 주소 정의
#define RCC_APB2ENR (*(volatile unsigned int *)0x40021018) 

#define GPIOA_CRL (*(volatile unsigned int *)0x40010800) 
#define GPIOB_CRH (*(volatile unsigned int *)0x40010C04) 
#define GPIOC_CRL (*(volatile unsigned int *)0x40011000) 
#define GPIOC_CRH (*(volatile unsigned int *)0x40011004) 

#define GPIOC_BSRR (*(volatile unsigned int *)0x40011010)

#define GPIOA_IDR (*(volatile unsigned int *)0x40010808) 
#define GPIOB_IDR (*(volatile unsigned int *)0x40010C08) 
#define GPIOC_IDR (*(volatile unsigned int *)0x40011008) 

#define RESET 0x44444444

// 딜레이 함수
void delay(){
  volatile int i;
  for(i=0; i<10000000; i++){}
}

int main(void)
{
    // [1] 클럭 활성화 (Port A, B, C)
    RCC_APB2ENR |= 0x3C;

    // [2] GPIO 초기화
    GPIOA_CRL = RESET;
    GPIOB_CRH = RESET;
    GPIOC_CRL = RESET;
    GPIOC_CRH = RESET;
    
    // 입력 핀 설정 (Input with pull-up/pull-down)
    GPIOA_CRL = 0x00000008; // PA0 (KEY4)
    GPIOB_CRH = 0x00000800; // PB10 (KEY2)
    GPIOC_CRH = 0x00800000; // PC13 (KEY3) - 초기화 중복 주의(아래에서 OR연산으로 모터핀 추가)
    GPIOC_CRL = 0x00080000; // PC4 (KEY1)

    // [3] 모터 출력 핀 설정 (Output Push-pull 50MHz: 0x3)
    // Motor L: PC8, PC9
    // Motor R: PC10, PC11
    // PC13은 입력으로 유지해야 하므로, 기존 설정(0x00800000)에 하위 비트(PC8~11)를 추가 설정
    // 0x3333 -> PC11(3), PC10(3), PC9(3), PC8(3)
    GPIOC_CRH |= 0x00003333; 
    
    unsigned int KEY1 = 0x0010; // PC4
    unsigned int KEY2 = 0x0400; // PB10
    unsigned int KEY3 = 0x2000; // PC13
    unsigned int KEY4 = 0x0001; // PA0

    /* * 모터 동작 논리 (BSRR 레지스터 활용)
     * BSRR 하위 16비트: 1로 설정 시 해당 핀 High (Set)
     * BSRR 상위 16비트: 1로 설정 시 해당 핀 Low (Reset)
     * * Motor L: PC8(정), PC9(역)
     * Motor R: PC10(정), PC11(역)
     */

    while(1){
        // KEY1: 전진 (Go Straight) -> 양쪽 모터 정방향
        // L: PC8=1, PC9=0 / R: PC10=1, PC11=0
        if((GPIOC_IDR & KEY1) == 0){ 
            // Set: 8, 10 / Reset: 9, 11
            // 0x00000500 (Set bit 8, 10) | 0x0A000000 (Reset bit 9, 11)
            GPIOC_BSRR = 0x0A000500; 
        }
        
        // KEY2: 후진 (Go Backward) -> 양쪽 모터 역방향
        // L: PC8=0, PC9=1 / R: PC10=0, PC11=1
        else if((GPIOB_IDR & KEY2) == 0){ 
            // Set: 9, 11 / Reset: 8, 10
            // 0x00000A00 (Set bit 9, 11) | 0x05000000 (Reset bit 8, 10)
            GPIOC_BSRR = 0x05000A00; 
        }
        
        // KEY3: 좌회전 (Turn Left) -> 왼쪽 모터 정지, 오른쪽 모터 전진
        // L: Stop / R: Forward
        // L: PC8=0, PC9=0 / R: PC10=1, PC11=0
        else if((GPIOC_IDR & KEY3) == 0){ 
            // Set: 10 / Reset: 8, 9, 11
            // 0x00000400 (Set bit 10) | 0x0B000000 (Reset bit 8, 9, 11)
            GPIOC_BSRR = 0x0B000400;
        }
        
        // KEY4: 정지 (Stop) -> 모든 모터 정지
        // L: Stop / R: Stop
        // All Low
        else if((GPIOA_IDR & KEY4) == 0){ 
            // Set: None / Reset: 8, 9, 10, 11
            // 0x0F000000 (Reset bit 8, 9, 10, 11)
            GPIOC_BSRR = 0x0F000000; 
        }
    }
    return 0;
}