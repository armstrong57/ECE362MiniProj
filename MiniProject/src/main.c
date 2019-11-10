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

#define s_0 0x3F
#define s_1 0x06
#define s_2 0x5B
#define s_3 0x4F
#define s_4 0x66
#define s_5 0x6D
#define s_6 0x7D
#define s_7 0x07
#define s_8 0x7F
#define s_9 0x67
#define SPI_DELAY 1337

int shiftCount = 0;
int ctChk = 0;
int seq = 0;

void nano_wait(unsigned int n) {
    //waits for n nanoseconds
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

void spi_cmd(char b) {
    //sends b to Data Register when the transmit buffer is empty
    // Your code goes here.
    while((SPI2->SR & SPI_SR_TXE) == 0);
    SPI2->DR = b;
}

void spi_data(char b) {
    // Your code goes here.
    while((SPI2->SR & SPI_SR_TXE) == 0);
    SPI2->DR = 0x200 | b;
}

void dma_spi_init(void) {
    // Your code goes here.
    //initializes DMA for SPI
    /*** WILL NEED TO BE ALTERED - NEED TO FIGURE OUT WHAT & HOW ***/
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CMAR = seq;
    DMA1_Channel5->CPAR = &SPI2->DR;
    DMA1_Channel5->CNDTR = 34;
    DMA1_Channel5->CCR |= DMA_CCR_DIR;
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0;
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0;
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    DMA1_Channel5->CCR &= ~DMA_CCR_PL;

    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

void bitbang_sendbit(int b) {
    const int SCK = 1<<13;
    const int MOSI = 1<<15;
    // We do this slowly to make sure we don't exceed the
    // speed of the device.
    GPIOB->BRR = SCK;
    if (b)
        GPIOB->BSRR = MOSI;
    else
        GPIOB->BRR = MOSI;
    //GPIOB->BSRR = b ? MOSI : (MOSI << 16);
    nano_wait(SPI_DELAY);
    GPIOB->BSRR = SCK;
    nano_wait(SPI_DELAY);
}

void bitbang_sendbyte(int b) {
    int x;
    // Send the eight bits of a byte to the SPI channel.
    // Send the MSB first (big endian bits).
    for(x=8; x>0; x--) {
        bitbang_sendbit(b & 0x80);
        b <<= 1;
    }
}

void spi_setup(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN; //enable clock to gpiob
    GPIOB->MODER &= ~(0xCF000000); //clear moder for pb12, pb13, pb15
    GPIOB->MODER |= 0x8A000000; //set moder to alternate function for pb12, pb13, pb15
    //set pb12, pb13, pb15 to alternate function 0
    GPIOB->AFR[1] &= ~(GPIO_AFRH_AFRH7 | GPIO_AFRH_AFRH5 | GPIO_AFRH_AFRH4);

    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; //enable clock to spi2
    //set bits in spi2->cr1: bidimode, bidioe, mstr, and set baud rate to 111.
    SPI2->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | SPI_CR1_MSTR |
            SPI_CR1_BR_1 | SPI_CR1_BR_0;
    SPI2->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA); //set cpol & cpha to 0.
    //set data size to 1001 (10-bit) and set NSSP & SSOE bits
    SPI2->CR2 = (SPI_CR2_DS_3 | SPI_CR2_DS_0 | SPI_CR2_NSSP | SPI_CR2_SSOE);

    SPI2->CR1 |= SPI_CR1_SPE; //enable spi2
}

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
}

void tim6_setup(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->ARR = 100;
    TIM6->PSC = 60000;
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1 |= TIM_CR1_CEN;

    NVIC->ISER[0] = 1 << TIM6_DAC_IRQn;
}

/*void TIM6_DAC_IRQHandler(void) {
    //each time: toggle shift clk(PC7)
    //every 2: toggle data (PC6), increment shiftCount
    //shiftCount = shiftSize: trigger rising store clk, set shiftCount = 0
    if(shiftseq[shiftCount] == -1) {
        GPIOC->ODR &= ~(1 << 6);
    } else {
        GPIOC->ODR |= (shiftseq[shiftCount] << 6);
    }
    shiftCount++;
    GPIOC->ODR |= 1 << 7;
    GPIOC->ODR &= ~1 << 7; //toggle PC7
    //GPIOC->ODR ^= 1 << 7;

    if(shiftCount % 2 == 0) {
        GPIOC->ODR ^= 1 << 9; //if we've shifted in the correct # bits, toggle store
    }
    if(shiftCount == 8) {
        shiftCount = 0;
        GPIOC->ODR &= ~(1 << 10);
        GPIOC->ODR |= 1 << 10;
    }
    TIM6->SR &= ~TIM_SR_UIF;
}*/

void sendDig(uint8_t val) {
    //send a digit to be shown on 7-seg
    GPIOA->ODR &= ~0xFF;
    GPIOA->ODR |= ~val;
}

int main(void)
{
    for(;;);
}
