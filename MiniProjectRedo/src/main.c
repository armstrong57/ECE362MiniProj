/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/


#include "stm32f0xx.h"
#include "stm32f0_discovery.h"

int get_key_press();
int get_key_release();
void TIM6_DAC_IRQHandler();
void setup_timer6();
void init_keypad();
int get_key_pressed();
char get_char_key();
void dma_spi_init();
void spi_setup();
void gpio_setup();
void trigger_alarm(int*);
char input_Digit(int);
void alarm_set();

#define s_0 0xC0
#define s_1 0xF9
#define s_2 0xA4
#define s_3 0xB0
#define s_4 0x99
#define s_5 0x92
#define s_6 0x82
#define s_7 0xF8
#define s_8 0x80
#define s_9 0x98
#define SPI_DELAY 1337

uint8_t seg7nums[10] = {s_0, s_1, s_2, s_3, s_4, s_5, s_6, s_7, s_8, s_9};
uint16_t spi_send[6];

int col = 0;
int8_t history[16] = {0};
int8_t lookup[16] = {1,4,7,0xe,2,5,8,0,3,6,9,0xf,0xa,0xb,0xc,0xd};
char char_lookup[16] = {'1','4','7','*','2','5','8','0','3','6','9','#','A','B','C','D'};
int alarm_hrT, alarm_mnT, alarm_scT, alarm_hrO, alarm_mnO, alarm_scO;
int alarmTime[6];

int hrs = 18;
int mins = 9;
int secs = 0;
int hrT, hrO, mnT, mnO, scT, scO;
uint16_t spi_send0, spi_send1, spi_send2, spi_send3, spi_send4, spi_send5;
uint8_t ss0, ss1, ss2, ss3, ss4, ss5;

int time_disp = 1;

int get_key_press() {
    while(1) {
        for(int i = 0; i < 16; i++) {
            if(history[i] == 1) {
                return i;
            }
        }
    }
}

int get_key_release() {
    while(1) {
        for(int i = 0; i < 16; i++) {
            if(history[i] == -2) {
                return i;
            }
        }
    }
}

void TIM6_DAC_IRQHandler() {
    TIM6->SR &= ~TIM_SR_UIF;
    int row = (GPIOA->IDR >> 4) & 0xf;
    int index = col << 2;
    history[index] = history[index] << 1;
    history[index] = history[index] | (row & 0x1);
    history[index + 1] = history[index + 1] << 1;
    history[index + 1] = history[index + 1] | ((row >> 1) & 0x1);
    history[index + 2] = history[index + 2] << 1;
    history[index + 2] = history[index + 2] | ((row >> 2) & 0x1);
    history[index + 3] = history[index + 3] << 1;
    history[index + 3] = history[index + 3] | ((row >> 3) & 0x1);
    col = col + 1;
    if(col > 3) {
        col = 0;
    }
    GPIOA->ODR = (1 << col);
}

void setup_timer6() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->PSC = 480 - 1;
    TIM6->ARR = 100 - 1;
    TIM6->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM6_DAC_IRQn, 192);
    NVIC->ISER[0] = 1 << TIM6_DAC_IRQn;
    TIM6->CR1 |= TIM_CR1_CEN;
}

void init_keypad() {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= ~0x0000ffff;
    GPIOA->MODER |= 0x00000055;
    GPIOA->PUPDR &= ~0x0000ff00;
    GPIOA->PUPDR |= 0x0000aa00;
}

int get_key_pressed() {
    int key = get_key_press();
    while(key != get_key_release());
    return key;
}

char get_char_key() {
    int index = get_key_pressed();
    return char_lookup[index];
}

//This function works
void dma_spi_init(void) {
    // Your code goes here.
    //initializes DMA for SPI
    /*** WILL NEED TO BE ALTERED - NEED TO FIGURE OUT WHAT & HOW ***/
    RCC->AHBENR |= RCC_AHBENR_DMA1EN; //enable clock to DMA
    DMA1_Channel5->CCR &= ~DMA_CCR_EN; //disable DMA
    DMA1_Channel5->CMAR = spi_send; //set memory address to spi_send
    DMA1_Channel5->CPAR = &SPI2->DR; //set peripheral address to SPI_DR
    DMA1_Channel5->CNDTR = 6; //sending 6 pieces of data - 1 16-bit vals per 7-seg
    DMA1_Channel5->CCR |= DMA_CCR_DIR; //read from memory
    DMA1_Channel5->CCR |= DMA_CCR_CIRC; //enable circular mode
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE; //clear msize
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0; //msize = 01, 16-bit
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE; //clear psize
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0; //psize = 01, 16-bit
    DMA1_Channel5->CCR |= DMA_CCR_MINC; //memory increment mode enable
    DMA1_Channel5->CCR |= DMA_CCR_PL_0 | DMA_CCR_PL_1; //priority = 11, very high

    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

//This function works
void spi_setup(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN; //enable clock to gpiob
    GPIOB->MODER &= ~(0xCF000000); //clear moder for pb12, pb13, pb15
    GPIOB->MODER |= 0x8A000000; //set moder to alternate function for pb12, pb13, pb15
    //set pb12, pb13, pb15 to alternate function 0
    GPIOB->AFR[1] &= ~(GPIO_AFRH_AFRH7 | GPIO_AFRH_AFRH5 | GPIO_AFRH_AFRH4);

    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; //enable clock to spi2
    //set bits in spi2->cr1: bidimode, bidioe, mstr, and set baud rate to 111.
    SPI2->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | SPI_CR1_MSTR | SPI_CR1_BR_2 |
            SPI_CR1_BR_1 | SPI_CR1_BR_0;
    SPI2->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA); //set cpol & cpha to 0.
    //set data size to 1001 (10-bit) and set NSSP & SSOE bits
    SPI2->CR2 = (SPI_CR2_DS_3 | SPI_CR2_DS_2 |  SPI_CR2_DS_1 | SPI_CR2_DS_0 | SPI_CR2_NSSP | SPI_CR2_SSOE);

    SPI2->CR1 |= SPI_CR1_SPE; //enable spi2
}

//This function works
void gpio_setup(void) {
    //setup gpio for direct write to 7-segs
//    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
//    GPIOA->MODER &= ~0xFFFF;
//    GPIOA->MODER |= 0x5555;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->BSRR = 1<<12; // set NSS high
    GPIOB->BRR = (1<<13) + (1<<15); // set SCK and MOSI low
    // Now, configure pins for output.
    GPIOB->MODER &= ~(3<<(2*12));
    GPIOB->MODER |=  (1<<(2*12));
    GPIOB->MODER &= ~( (3<<(2*13)) | (3<<(2*15)) );
    GPIOB->MODER |=    (1<<(2*13)) | (1<<(2*15));

    //LED output for testing alarm
    GPIOB->MODER &= ~(3 << (2 * 5));
    GPIOB->MODER |= 1 << (2 * 5);

    //LEDs for checking alarm input
    GPIOB->MODER &= ~(3 << (2 * 4) | 3 << (2 * 6));
    GPIOB->MODER |= 1 << (2 * 4) | 1 << (2 * 6);
}

//This function works
void tim15_setup(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    //arr = 100, psc = 8000 --> fast mode (1 min / s)
    //arr = 1000, psc = 48000 --> normal mode (1s/s)
    TIM15->ARR = 1000 - 1;
    TIM15->PSC = 48000 - 1;
    TIM15->DIER |= TIM_DIER_UIE;
    TIM15->CR1 |= TIM_CR1_CEN;

    NVIC_SetPriority(TIM15_IRQn, 0);
    NVIC->ISER[0] = 1 << TIM15_IRQn;
}

//This function works
void TIM15_IRQHandler(void) {
    secs++;
    if(secs == 60) {
        secs = 0;
        mins++;
    }
    if(mins == 60) {
        mins = 0;
        hrs++;
    }
    if(hrs == 24) {
        hrs = 0;
    }

    hrT = hrs / 10;
    hrO = hrs % 10;
    mnT = mins / 10;
    mnO = mins % 10;
    scT = secs / 10;
    scO = secs % 10;

    ss0 = seg7nums[hrT];
    ss1 = seg7nums[hrO];
    ss2 = seg7nums[mnT];
    ss3 = seg7nums[mnO];
    ss4 = seg7nums[scT];
    ss5 = seg7nums[scO];

    spi_send0 = (ss0 << 8) | (0xFE); //11111011
    spi_send1 = (ss1 << 8) | (0xFD); //11110111
    spi_send2 = (ss2 << 8) | (0xFB); //11101111
    spi_send3 = (ss3 << 8) | (0xF7); //11011111
    spi_send4 = (ss4 << 8) | (0xEF); //10111111
    spi_send5 = (ss5 << 8) | (0xDF); //01111111

    spi_send[0] = spi_send0;
    spi_send[1] = spi_send1;
    spi_send[2] = spi_send2;
    spi_send[3] = spi_send3;
    spi_send[4] = spi_send4;
    spi_send[5] = spi_send5;

    TIM15->SR &= ~TIM_SR_UIF;
}

//This function works
void trigger_alarm(int * time) {
    while(1) {
        if(hrs == (time[0] * 10 + time[1]) &&
                mins == (time[2] * 10 + time[3]) &&
                secs == (time[4] * 10 + time[5])) {
            GPIOB->ODR |= 1 << 5;
            micro_wait(200000);
            GPIOB->ODR &= ~(1 << 5);
            return;
        }
    }
}

//Check this function
char input_Digit (int pos) {
    //pos == 0: hrT
    //pos == 1: hrO
    //pos == 2: mnT
    //pos == 3: mnO
    //pos == 4: scT
    //pos == 5: scO
    GPIOB->ODR &= ~(1 << 4);
    char inDig;
    inDig = get_char_key(); //this will be from Sam's code
    if(pos == 0) {
        //ss0 = seg7nums[inDig];
        //spi_send0 = (ss0 << 8) | (0xFE); //11111011
        //spi_send[0] = spi_send0;
        while (inDig > '2') {
            inDig = get_char_key();
            //GPIOB->ODR |= 1 << 6;
            //ss0 = seg7nums[inDig];
            //spi_send0 = (ss0 << 8) | (0xFE); //11111011
            //spi_send[0] = spi_send0;
        }
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    } if(pos == 1) {
        //ss1 = seg7nums[inDig];
        //spi_send1 = (ss1 << 8) | (0xFD); //11110111
        //spi_send[1] = spi_send1;
        while (alarm_hrT == '2' && inDig > '3') {
            inDig = get_char_key();
            //ss1 = seg7nums[inDig];
            //spi_send1 = (ss1 << 8) | (0xFD); //11110111
            //spi_send[1] = spi_send1;

        }
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    } if(pos == 2) {
        //ss2 = seg7nums[inDig];
        //spi_send2 = (ss2 << 8) | (0xFB); //11101111
        //spi_send[2] = spi_send2;
        while(inDig > '5') {
            inDig = get_char_key();
            //ss2 = seg7nums[inDig];
            //spi_send2 = (ss2 << 8) | (0xFB); //11101111
            //spi_send[2] = spi_send2;
        }
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    } if(pos == 3) {
        //ss3 = seg7nums[inDig];
        //spi_send3 = (ss3 << 8) | (0xF7); //11011111
        //spi_send[3] = spi_send3;
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    } if(pos == 4) {
        while(inDig > '5') {
            inDig = get_char_key();
        }
        //ss4 = seg7nums[inDig];
        //spi_send4 = (ss4 << 8) | (0xEF); //11011111
        //spi_send[4] = spi_send4;
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    } if(pos == 5) {
        //ss5 = seg7nums[inDig];
        //spi_send5 = (ss5 << 8) | (0xDF); //11011111
        //spi_send[5] = spi_send5;
        GPIOB->ODR |= 1 << 4;
        micro_wait(200000);
        return inDig - 48;
    }
    return 0;
}

//Check this function
void alarm_set() {
    alarm_hrT = input_Digit(0);
    alarm_hrO = input_Digit(1);
    alarm_mnT = input_Digit(2);
    alarm_mnO = input_Digit(3);
    alarm_scT = input_Digit(4);
    alarm_scO = input_Digit(5);
    //GPIOB->ODR |= 1 << 4;
    //micro_wait(500000);
    GPIOB->ODR &= ~(1 << 4);
    alarmTime[0] = alarm_hrT;
    alarmTime[1] = alarm_hrO;
    alarmTime[2] = alarm_mnT;
    alarmTime[3] = alarm_mnO;
    alarmTime[4] = alarm_scT;
    alarmTime[5] = alarm_scO;
}

void set_alarm_time(void) {
    while(1) {
        char check = get_char_key();
        if(check == '*') {
            GPIOB->ODR |= 1 << 4;
            micro_wait(200000);
            alarm_set();
            return;
        }
    }
}

int main(void)
{
    //figure out variables for current time, input time
    //int hrs, mins, secs;

    gpio_setup();
    spi_setup();
    dma_spi_init();
    tim15_setup();
    setup_timer6();
    init_keypad();
    while(1) {
        set_alarm_time();
        trigger_alarm(alarmTime);
    }
    for(;;);
}
