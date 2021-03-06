#include <xc.h>           // processor SFR definitions
#include <sys/attribs.h>  // __ISR macro
#include "ILI9163C.h"     // SPI and LCD commands

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

void LCD_drawChar(unsigned short, unsigned short, char);

// LCD functions//
void LCD_drawChar(unsigned short x2, unsigned short y2, char symbol){
    int set, x, y, ascii_row;
    int col = 0;
    int bit_index = 0;
    char bit_map;
    
    ascii_row = (int)(symbol - 32);
    
    while (col < 5){
        bit_index = 0;
        bit_map = ASCII[ascii_row][col];
        while (bit_index < 8){
            set = (bit_map >> bit_index) & 0x01;
            x = x2 + col;
            y = y2 + bit_index;
            if (set){
                LCD_drawPixel(x, y, YELLOW);
            }
            else{
                LCD_drawPixel(x, y, BLACK);
            }
            bit_index++;
        }
        col++;
    }
}

void LCD_drawString(unsigned short x, unsigned short y, char *array){
    int i = 0;
    int begin_pos = x;
    while (array[i]!=0){
        if (array[i]=='\n'){
            y = y + 10;
            x = begin_pos;
            i++;
            continue;
        }
        LCD_drawChar(x,y,array[i]);
        x = x + 6;
        i++;
    }
}

int main() {

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
    SPI1_init();
    LCD_init();
    LCD_clearScreen(BLACK);
    
    //unsigned short a = 0x0000;
    //char test;
    //unsigned short test1 = 2;
    //unsigned short test2 = 3;
    int x = 1337;
    char array[100];
    
    __builtin_enable_interrupts();
    
    while(1) {
	    // use _CP0_SET_COUNT(0) and _CP0_GET_COUNT() to test the PIC timing
		// remember the core timer runs at half the CPU speed
        _CP0_SET_COUNT(0);                   // set core timer to 0
        LATAbits.LATA4 = 0;       // intialize LED on
        //LCD_clearScreen(BLACK);
        //LCD_drawPixel(test1 + 4,test2 + 4,GREEN);
        //test = '!';
        sprintf(array,"HELLO WORLD %i!",x);
        LCD_drawString(28,32,array);
        //LCD_drawChar(10,10,'S');
        
        
        
        while (_CP0_GET_COUNT() < 48000000){;}
                

    }  
    
}

