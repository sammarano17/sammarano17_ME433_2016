#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include <stdio.h>
#include <math.h> 
#include "ILI9163C.h"

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
#pragma config OSCIOFNC = ON // free up secondary osc pins
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
#pragma config UPLLIDIV =  DIV_2 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = ON // USB clock on

// DEVCFG3
#pragma config USERID = 0x1000 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY=  OFF // allow multiple reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module

#define IMU_ADDRESS 0b1101011
#define OUT_TEMP_L 0x20

//CTRL1,CTRL2,CTRL3 initialize values//
#define CTRL1_XL 0b10000001
#define CTRL2_G  0b10000000
#define CTRL3_C  0b00000100

//I2C functions//
void initI2C2(void){
    ANSELBbits.ANSB2 = 0;
    ANSELBbits.ANSB3 = 0;
    //some number for 100kHz; // I2CBRG = [1/(2*Fsck) - PGD]*Pblck - 2
    I2C2BRG = 233;           // PGD = 104ns, Fsck = 100kHz, Pblck = 48MHz.
    I2C2CONbits.ON = 1;      // turn on the I2C2 module
}

void i2c_master_start(void) {
    I2C2CONbits.SEN = 1;            // send the start bit
    while(I2C2CONbits.SEN) { ; }    // wait for the start bit to be sent
}

void i2c_master_restart(void) {
    I2C2CONbits.RSEN = 1;           // send a restart
    while(I2C2CONbits.RSEN) { ; }   // wait for the restart to clear
}

void i2c_master_send(unsigned char byte) { // send a byte to slave
  I2C2TRN = byte;                   // if an address, bit 0 = 0 for write, 1 for read
  while(I2C2STATbits.TRSTAT) { ; }  // wait for the transmission to finish
  if(I2C2STATbits.ACKSTAT) {        // if this is high, slave has not acknowledged
    // ("I2C2 Master: failed to receive ACK\r\n");
  }
}

unsigned char i2c_master_recv(void) { // receive a byte from the slave
    I2C2CONbits.RCEN = 1;             // start receiving data
    while(!I2C2STATbits.RBF) { ; }    // wait to receive the data
    return I2C2RCV;                   // read and return the data
}

void i2c_master_ack(int val) {        // sends ACK = 0 (slave should send another byte)
                                      // or NACK = 1 (no more bytes requested from slave)
    I2C2CONbits.ACKDT = val;          // store ACK/NACK in ACKDT
    I2C2CONbits.ACKEN = 1;            // send ACKDT
    while(I2C2CONbits.ACKEN) { ; }    // wait for ACK/NACK to be sent
}

void i2c_master_stop(void) {          // send a STOP:
  I2C2CONbits.PEN = 1;                // comm is complete and master relinquishes bus
  while(I2C2CONbits.PEN) { ; }        // wait for STOP to complete
}

//function initializations//
unsigned char readIMU(char reg);
void init_IMU(void);
void I2C_read_multiple(char address, char Register, unsigned char * data, char length);
void LCD_drawString(unsigned short x, unsigned short y, char *array);

//variable init//
    unsigned char test;
    unsigned char output[14];
    signed short temp = 1;
    signed short g_x,g_y,g_z,xl_x,xl_y,xl_z;
    char array[100];


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
    LATAbits.LATA4 = 0;
    
    initI2C2();
    SPI1_init();
    LCD_init();
    
    __builtin_enable_interrupts();
    
        
    while(1) {
	    // use _CP0_SET_COUNT(0) and _CP0_GET_COUNT() to test the PIC timing
		// remember the core timer runs at half the CPU speed
        _CP0_SET_COUNT(0);                   // set core timer to 0
        while (_CP0_GET_COUNT() < 480000){;} // read at 50 Hz -- 480k / 24 MHz
               // intialize LED on
        
        //I2C_read_multiple(IMU_ADDRESS<<1,OUT_TEMP_L,output,14);
        
        //temp = (output[0] | (output[1] << 8));
        //g_x = (output[2] | (output[3] << 8));
        //g_y = (output[4] | (output[5] << 8));
        //g_z = (output[6] | (output[7] << 8));
        //xl_x = (output[8] | (output[9] << 8));
        //xl_y = (output[10] | (output[11] << 8));
        //xl_z = (output[14] | (output[13] << 8));
        
        //output[0] = readIMU(0x20);
        //output[1] = readIMU(0x21);
        
        //temp = (output[0] | (output[1] << 8));
        
        //sprintf(array,"TEMP: %i",temp);
        //LCD_drawString(2,22,array);
        
        //sprintf(array,"XL_X: %i",xl_x);
        //LCD_drawString(2,2,array);
        //sprintf(array,"XL_Y: %i",xl_y);
        //LCD_drawString(2,12,array);
        //sprintf(array,"XL_Z: %i",xl_z);
        //LCD_drawString(2,22,array);
        //sprintf(array,"G_X: %i",g_x);
        //LCD_drawString(2,32,array);
        //sprintf(array,"G_Y: %i",g_y);
        //LCD_drawString(2,42,array);
        //sprintf(array,"G_Z: %i",g_z);
        //LCD_drawString(2,52,array);
        //sprintf(array,"TEMP: %i",temp);
        //LCD_drawString(2,62,array);
        
        test = readIMU(0x0F);
        sprintf(array,"WHO: %i",test);
        LCD_drawString(2,22,array);
        
        //if (test==0b01101001){
        //    LATAbits.LATA4 = 1;
        //}
        //else {
        //    LATAbits.LATA4 = 0;
        //}
    }
}


//IMU Setup//

unsigned char readIMU(char reg){
    //LATAbits.LATA4 = 1;
    i2c_master_start();
    i2c_master_send(IMU_ADDRESS<<1);
    i2c_master_send(reg);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    unsigned char r = i2c_master_recv();
    i2c_master_ack(1); 
    i2c_master_stop(); 
    return r;
}

void init_IMU(void){
    //init_XL//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x10); // the register to write to
    i2c_master_send(CTRL1_XL); // the value to put in the register
    i2c_master_stop(); // make the stop bit
    //init_G//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x11); // the register to write to
    i2c_master_send(CTRL2_G); // the value to put in the register
    i2c_master_stop(); // make the stop bit
    //init_C//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x12); // the register to write to
    i2c_master_send(CTRL3_C); // the value to put in the register
    i2c_master_stop(); // make the stop bit
}

void I2C_read_multiple(char address, char Register, unsigned char * data, char length){
    int i = 0;
    i2c_master_start();
    i2c_master_send(address);
    i2c_master_send(Register);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    for (i=0;i<=length;i++){
    output[i] = i2c_master_recv();
    if (i<length){
        i2c_master_ack(0);
    }
    else if (i==length){
        i2c_master_ack(1);
    }
    } 
    i2c_master_stop(); 
}


//LCD functions//
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