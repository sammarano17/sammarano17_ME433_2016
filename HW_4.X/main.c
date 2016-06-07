#include <xc.h>           // processor SFR definitions
#include <sys/attribs.h>  // __ISR macro
#include <math.h>
#include "spi_dac.h"
#include "i2c.h"

// DEVCFG0
#pragma config DEBUG = OFF // no debugging
#pragma config JTAGEN = OFF // no jtag
#pragma config ICESEL = ICS_PGx1 // use PGED1 and PGEC1
#pragma config PWP = OFF // no write protect
#pragma config BWP = OFF // no boot write protect
#pragma config CP = OFF // no code protect

// DEVCFG1
#pragma config FNOSC = PRIPLL // use primary oscillator with pll
#pragma config FSOSCEN = OFF // turn off secondary oscillator
#pragma config IESO = OFF // no switching clocks
#pragma config POSCMOD = HS // high speed crystal mode
#pragma config OSCIOFNC = OFF // free up secondary osc pins
#pragma config FPBDIV = DIV_1 // divide CPU freq by 1 for peripheral bus clock
#pragma config FCKSM = CSDCMD // do not enable clock switch
#pragma config WDTPS = PS1048576 // slowest wdt
#pragma config WINDIS = OFF // no wdt window
#pragma config FWDTEN = OFF // wdt off by default
#pragma config FWDTWINSZ = WINSZ_25 // wdt window at 25%

// DEVCFG2 - get the CPU clock to 48MHz
#pragma config FPLLIDIV = DIV_2 // divide input clock to be in range 4-5MHz
#pragma config FPLLMUL = MUL_24 // multiply clock after FPLLIDIV
#pragma config FPLLODIV = DIV_2 // divide clock after FPLLMUL to get 48MHz
#pragma config UPLLIDIV = DIV_2 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = ON // USB clock on

// DEVCFG3
#pragma config USERID = 0xFFFF // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY = OFF // allow multiple reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module

#define CS LATBbits.LATB7       // chip select pin


int sinewave[100];
int triangle_wave[200];


void initExpander(void){    // GP0-3 outputs and GP4-7 inputs
  i2c_master_start();
  i2c_master_send(0x40);   // GPIO address & indicate write
  i2c_master_send(0x00);   // addr of I/O direction register
  i2c_master_send(0xF0);   // send the value to the register. 0-3outputs 4-7 inputs
  i2c_master_stop();
}

void setVoltage(unsigned char channel, unsigned char voltage){    //channel 0 for voutA, 1 for voutB
    unsigned short value = 0;
    if(channel == 0){
      value = (0b0011 << 12) + (voltage << 4);
        CS = 0;                                 // listen to me
        SPI1_IO((value & 0xFF00) >> 8 ); // most significant byte of address
        SPI1_IO(value & 0x00FF);         // the least significant address byte
        CS = 1;                          // end
    }
    if (channel == 1){
      value = (0b1011 << 12) + (voltage << 4);
        CS = 0;
        SPI1_IO((value & 0xFF00) >> 8 ); // most significant byte of address
        SPI1_IO(value & 0x00FF);         // the least significant address byte
        CS = 1;
    }
}

char getExpander(void){ // read from GP7
  
  i2c_master_start();
  i2c_master_send(0x40);
  i2c_master_send(0x09);  // the register to read from (GPIO)
  i2c_master_restart();   // make the restart bit, so we can begin reading
  i2c_master_send(0x41); // indicate reading
  
  unsigned char read = i2c_master_recv() >> 7;     // save the value returned. the value of GP7
  
  i2c_master_ack(1); // make the ack so the slave knows we got it
  i2c_master_stop(); // make the stop bit
  //read = 1;
  return read;
}

void setExpander(char pin, char level){ // set GP0
  
  i2c_master_start();
  i2c_master_send(0x40);   // GPIO address & indicate write
  i2c_master_send(0x0A);   // addr of OLAT register

  if(level == 1){
    //LATAbits.LATA4 = 0;
    i2c_master_send(0x01);   // send '1' to GP0, indicating high level.
  }
  if(level == 0){
    i2c_master_send(0x00);
  }
  //i2c_master_send(0x0F);
  i2c_master_stop();
}

void waveGenerator(void){
  int i;
  for(i = 0; i < 100; i++){
    sinewave[i] = (int)(128.0 + 127.5 * sin(M_PI * 0.02 * i));
  }
  for(i = 0; i < 200; i++){
    triangle_wave[i] = (int)(1.28 * i);
  }
}


int main() {
    //int value = 0;
    char data;
    waveGenerator();
    int count1 = 0;
    int count2 = 0;
    __builtin_disable_interrupts();

    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);

    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;

    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;

    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;
    
    // do your TRIS and LAT commands here
    TRISAbits.TRISA4 = 0;     // ouput
    TRISBbits.TRISB4 = 1;     // input

    //LATAbits.LATA4 = 1;       // intialize LED on
    initSPI1();
    initI2C2();
    initExpander();
    
    __builtin_enable_interrupts();
    
    while(1) {
	    // use _CP0_SET_COUNT(0) and _CP0_GET_COUNT() to test the PIC timing
		// remember the core timer runs at half the CPU speed
        _CP0_SET_COUNT(0);                   // set core timer to 0
        LATAbits.LATA4 = 1;       // intialize LED on
        setVoltage(0,sinewave[count1]);
        setVoltage(1,triangle_wave[count2]);
        count1++;
        count2++;
        if(count1 == 100){
          count1 = 0;
        }
        if(count2 == 200){
          count2 = 0;
        }
        data = getExpander();
        //data = (char)0x01;
        setExpander(0,data);
        
        while(_CP0_GET_COUNT() < 24000){     // wait 1ms / 0.001s
            ;
        }

    }  
    
}