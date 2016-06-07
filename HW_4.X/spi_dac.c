#include "spi_dac.h"
#include<xc.h> 
#define CS LATBbits.LATB7       // chip select pin

void initSPI1(void){
  // set up the chip select pin as an output
  // the chip select pin is used by the sram to indicate
  // when a command is beginning (clear CS to low) and when it
  // is ending (set CS high)
  TRISBbits.TRISB7 = 0;     // set RB7 output : chip selection
  TRISBbits.TRISB8 = 0;     // set RB8 output : SDO1
                            // set RB9 output : SDI1(no need)
  CS = 1;                   // disabled the slave
  RPB8Rbits.RPB8R = 0b0011; // assign SDO1 to PIN 17

  // Master - SPI4, pins are: SDI4(F4), SDO4(F5), SCK4(F13).  
  SPI1CON = 0;              // turn off the spi module and reset it
  SPI1BUF;                  // clear the rx buffer by reading from it
  SPI1BRG = 0x3;            // baud rate to 10 MHz [SPI4BRG = (80000000/(2*desired))-1]
  SPI1STATbits.SPIROV = 0;  // clear the overflow bit
  SPI1CONbits.CKE = 1;      // data changes when clock goes from hi to lo (since CKP is 0)
  SPI1CONbits.MSTEN = 1;    // master operation
  SPI1CONbits.ON = 1;       // turn on spi 4

}


char SPI1_IO(char write){
    SPI1BUF = write;
    while(!SPI1STATbits.SPIRBF){
        ;
    }
    return SPI1BUF;
}