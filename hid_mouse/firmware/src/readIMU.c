#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include <stdio.h>
#include <math.h> 
#include "readIMU.h"

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
    signed short scale = 1000;
    signed short g_x,g_y,g_z,xl_x,xl_y,xl_z,temp;
    float xl_xf,xl_yf,xl_zf,g_xf,g_yf,g_zf,tempf;
    char array[100];
static unsigned char pGammaSet[15]= {0x36,0x29,0x12,0x22,0x1C,0x15,0x42,0xB7,0x2F,0x13,0x12,0x0A,0x11,0x0B,0x06};
static unsigned char nGammaSet[15]= {0x09,0x16,0x2D,0x0D,0x13,0x15,0x40,0x48,0x53,0x0C,0x1D,0x25,0x2E,0x34,0x39};


float Read_IMU_Mouse(char reg) {
    //__builtin_disable_interrupts();

    initI2C2();
    SPI1_init();
    LCD_init();
    init_IMU();
    //LCD_clearScreen(BLACK);
    
    //__builtin_enable_interrupts();
    
        
	    // use _CP0_SET_COUNT(0) and _CP0_GET_COUNT() to test the PIC timing
		// remember the core timer runs at half the CPU speed
        _CP0_SET_COUNT(0);                   // set core timer to 0
        
        while (_CP0_GET_COUNT() < 480000){;} // read at 50 Hz -- 480k / 24 MHz
               // intialize LED on
        
        
        I2C_read_multiple(IMU_ADDRESS<<1,reg,output,2);
        
        temp = (output[0] | (output[1] << 8));
        
        tempf = ((float)temp)/16383;
        
        //tempf = ((float)temp)/16383;
        //sprintf(array,"XL(g): %f   ",tempf);
        //LCD_drawString(2,2,array);
      
        return tempf;
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

void SPI1_init() {
	SDI1Rbits.SDI1R = 0b0100; // B8 is SDI1
    RPA1Rbits.RPA1R = 0b0011; // A1 is SDO1
    TRISBbits.TRISB7 = 0; // SS is B7
    LATBbits.LATB7 = 1; // SS starts high

    // A0 / DAT pin
    ANSELBbits.ANSB15 = 0;
    TRISBbits.TRISB15 = 0;
    LATBbits.LATB15 = 0;
	
	SPI1CON = 0; // turn off the spi module and reset it
    SPI1BUF; // clear the rx buffer by reading from it
    SPI1BRG = 1; // baud rate to 12 MHz [SPI1BRG = (48000000/(2*desired))-1]
    SPI1STATbits.SPIROV = 0; // clear the overflow bit
    SPI1CONbits.CKE = 1; // data changes when clock goes from hi to lo (since CKP is 0)
    SPI1CONbits.MSTEN = 1; // master operation
    SPI1CONbits.ON = 1; // turn on spi1
}

unsigned char spi_io(unsigned char o) {
  SPI1BUF = o;
  while(!SPI1STATbits.SPIRBF) { // wait to receive the byte
    ;
  }
  return SPI1BUF;
}

void LCD_command(unsigned char com) {
    LATBbits.LATB15 = 0; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(com);
    LATBbits.LATB7 = 1; // CS
}

void LCD_data(unsigned char dat) {
    LATBbits.LATB15 = 1; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(dat);
    LATBbits.LATB7 = 1; // CS
}

void LCD_data16(unsigned short dat) {
    LATBbits.LATB15 = 1; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(dat>>8);
    spi_io(dat);
    LATBbits.LATB7 = 1; // CS
}

void LCD_init() {
    int time = 0;
    LCD_command(CMD_SWRESET);//software reset
    time = _CP0_GET_COUNT();
    while (_CP0_GET_COUNT() < time + 48000000/2/2) {} //delay(500);

	LCD_command(CMD_SLPOUT);//exit sleep
    time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/200) {} //delay(5);

	LCD_command(CMD_PIXFMT);//Set Color Format 16bit
	LCD_data(0x05);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/200) {} //delay(5);

	LCD_command(CMD_GAMMASET);//default gamma curve 3
	LCD_data(0x04);//0x04
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_GAMRSEL);//Enable Gamma adj
	LCD_data(0x01);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_NORML);

	LCD_command(CMD_DFUNCTR);
	LCD_data(0b11111111);
	LCD_data(0b00000110);

    int i = 0;
	LCD_command(CMD_PGAMMAC);//Positive Gamma Correction Setting
	for (i=0;i<15;i++){
		LCD_data(pGammaSet[i]);
	}

	LCD_command(CMD_NGAMMAC);//Negative Gamma Correction Setting
	for (i=0;i<15;i++){
		LCD_data(nGammaSet[i]);
	}

	LCD_command(CMD_FRMCTR1);//Frame Rate Control (In normal mode/Full colors)
	LCD_data(0x08);//0x0C//0x08
	LCD_data(0x02);//0x14//0x08
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_DINVCTR);//display inversion
	LCD_data(0x07);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_PWCTR1);//Set VRH1[4:0] & VC[2:0] for VCI1 & GVDD
	LCD_data(0x0A);//4.30 - 0x0A
	LCD_data(0x02);//0x05
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_PWCTR2);//Set BT[2:0] for AVDD & VCL & VGH & VGL
	LCD_data(0x02);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_VCOMCTR1);//Set VMH[6:0] & VML[6:0] for VOMH & VCOML
	LCD_data(0x50);//0x50
	LCD_data(99);//0x5b
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_VCOMOFFS);
	LCD_data(0);//0x40
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_CLMADRS);//Set Column Address
	LCD_data16(0x00);
    LCD_data16(_GRAMWIDTH);

	LCD_command(CMD_PGEADRS);//Set Page Address
	LCD_data16(0x00);
    LCD_data16(_GRAMHEIGH);

	LCD_command(CMD_VSCLLDEF);
	LCD_data16(0); // __OFFSET
	LCD_data16(_GRAMHEIGH); // _GRAMHEIGH - __OFFSET
	LCD_data16(0);

	LCD_command(CMD_MADCTL); // rotation
    LCD_data(0b00001000); // bit 3 0 for RGB, 1 for GBR, rotation: 0b00001000, 0b01101000, 0b11001000, 0b10101000

	LCD_command(CMD_DISPON);//display ON
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_RAMWR);//Memory Write
}

void LCD_drawPixel(unsigned short x, unsigned short y, unsigned short color) {
    // check boundary
    LCD_setAddr(x,y,x+1,y+1);
    LCD_data16(color);
}

void LCD_setAddr(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1) {
    LCD_command(CMD_CLMADRS); // Column
    LCD_data16(x0);
	LCD_data16(x1);

	LCD_command(CMD_PGEADRS); // Page
	LCD_data16(y0);
	LCD_data16(y1);

	LCD_command(CMD_RAMWR); //Into RAM
}

void LCD_clearScreen(unsigned short color) {
    int i;
    LCD_setAddr(0,0,_GRAMWIDTH,_GRAMHEIGH);
		for (i = 0;i < _GRAMSIZE; i++){
			LCD_data16(color);
		}
}