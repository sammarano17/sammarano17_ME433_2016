#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include <stdio.h>
#include <math.h> // used in HW4
#define CS LATBbits.LATB7   // chip select pin
#define PI 3.14159265 // used in HW4

// addresses used for IMU
#define WHO_AM_I 0x0F
#define IMU_ADDR 0b11010110 // full 8 bit address, has pull up resistors
#define IMU_7bit 0b1101011 // 7 bit address - shift over for 1/0 r/w
#define CTRL1_XL 0x10 // turn on accelerometer
#define CTRL2_G 0x11 // turn on gyroscope
#define CTRL3_C 0x12 // turn on IF_INC --- enable multiple read
#define OUT_TEMP_L 0x20 // call this for multiple read

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

////////////////////////// function prototypes /////////////////////////////////
void PIC32_init(void);

// i2c functions
void i2c_master_start(void);
void i2c_master_restart(void);
void i2c_master_send(unsigned char byte);
unsigned char i2c_master_recv(void);
void i2c_master_ack(int val);
void i2c_master_stop(void);
void i2c_master_setup(void);

// IMU functions
void imu_setup(void);
unsigned char I2C_read_single(void);
void I2C_read_multiple(char, char, unsigned char *, char);
void bit_shift(void);
void OC_setup(void);

// initialize variables
unsigned char outputs[14]; // length 14 b/c 14 registers to read from linearly
signed short temp = 0;
signed short g_x = 0, g_y = 0, g_z = 0;
signed short xl_x = 0, xl_y = 0, xl_z = 0;

int main() {

    __builtin_disable_interrupts();

    PIC32_init();
    i2c_master_setup();
    imu_setup();
    OC_setup();

    __builtin_enable_interrupts();

    // // for debugging - make sure code is working
    // // remember: TRIS = set I/O, LAT = write to bit
     TRISBbits.TRISB4 = 1; // Set pushbutton (B4) as input
     TRISAbits.TRISA4 = 0; // Set green LED(A4) as output ON
     LATAbits.LATA4 = 0;

    //unsigned char values; // store value returned from who_am_i register, unused

    while(1) {


        _CP0_SET_COUNT(0); // core timer = 0, runs at half CPU speed
        while (_CP0_GET_COUNT() < 480000){;} // read at 50 Hz -- 480k / 24 MHz

        //values = I2C_read_single(); // read from who_am_i register
        I2C_read_multiple(IMU_7bit, OUT_TEMP_L, outputs, 14); // multiple reads
        // data is read from OUT_TEMP_L, moving "upward" -- 14 registers total
        temp = (outputs[0] | (outputs[1] << 8));
        g_x = (outputs[2] | (outputs[3] << 8));
        g_y = (outputs[4] | (outputs[5] << 8));
        g_z = (outputs[6] | (outputs[7] << 8));
        xl_x = (outputs[8] | (outputs[9] << 8));
        xl_y = (outputs[10] | (outputs[11] << 8));
        xl_z = (outputs[14] | (outputs[13] << 8));

        if (values == 0b01101001){ // who am i register working!!
            LATAbits.LATA4 = 1;
        }
        else {
            LATAbits.LATA4 = 0;
        }

        ///// TODO: Check so max out at 100% duty cycle? ////

        // max of short = 65535 --- make sure to scale up to +/- 1g
        // xl scale = +/- 2g, multiply by 2
        // 32k based on 50% duty cycle for 0 acceleration
        float OC1_val = (6000.0/65535.0)*(2.0*xl_x + 32767.0);
        float OC2_val = (6000.0/65535.0)*(2.0*xl_y + 32767.0);
        OC1RS = (int) OC1_val; // typecast since OC1RS should be an int
        OC2RS = (int) OC2_val;
    }
}

////////////////////////////////// PIC 32 INITIALIZATION ///////////////////////
void PIC32_init(void){
    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);
    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;
    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;
    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;
}

/////////////////////////////////// I2C ///////////////////////////////////////
void i2c_master_setup(void) {
    // maximum baud = 400 kHz
    // use 100 kHz for now, use 400 kHz later
    // Fsck = 100 kHz, PGD = 104 ns (the typical value, on page 269), Pblck = 48 MHz
    I2C2BRG = 233;            // I2CBRG = [1/(2*Fsck) - PGD]*Pblck - 2
                                    // look up PGD for your PIC32
    I2C2CONbits.ON = 1;               // turn on the I2C2 module

    // I2C2 are analog inputs by default --- turn them off
    ANSELBbits.ANSB2 = 0;
    ANSELBbits.ANSB3 = 0;
}

// Start a transmission on the I2C bus
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

//////////////////////////////// IMU ///////////////////////////////////////////
void imu_setup(void){
    // accelerometer set up
    //  Set the sample rate to 1.66 kHz, 2g sensitivity, x filter.
    unsigned char xl_setup = 0b10000000;
    i2c_master_start();
    i2c_master_send(IMU_ADDR);
    i2c_master_send(CTRL1_XL);
    i2c_master_send(xl_setup);
    i2c_master_stop();

    // gyroscope set up
    // ample rate to 1.66 kHz, 245 dps sensitivity, x filter.
    unsigned char g_setup = 0b10000000;
    i2c_master_start();
    i2c_master_send(IMU_ADDR);
    i2c_master_send(CTRL2_G);
    i2c_master_send(g_setup);

    // multiple read set up
    // if_inc bit must be 1 to enable
    unsigned char read_setup = 0b00000100;
    i2c_master_start();
    i2c_master_send(IMU_ADDR);
    i2c_master_send(CTRL3_C);
    i2c_master_send(read_setup);
    i2c_master_stop();
}

// currently written to read from the who_am_i register
unsigned char I2C_read_single(void){
    i2c_master_start();
    i2c_master_send(IMU_ADDR);
    i2c_master_send(WHO_AM_I); // read from the who_am_i register to get logic
    i2c_master_restart();
    i2c_master_send(0b11010111); // send the read command, 1 lsb means read
    unsigned char r = i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    return r;
}

void I2C_read_multiple(char address, char reg, unsigned char * data, char length){
    i2c_master_start();
    i2c_master_send((address << 1)); // shift address 1 -- 0 in lsb = write
    i2c_master_send(reg); // this should be out_temp_L
    i2c_master_restart();
    i2c_master_send((address<< 1)| 0x01); // put a 1 in lsb = read

    int i;
    for (i = 0; i < length; i++){ // go through the 14 registers, starting at out_temp_l
        outputs[i] = i2c_master_recv();
        if((i+1) == length){ // statement should be true for last iteration
            i2c_master_ack(1); // or NACK = 1 (no more bytes requested from slave)
        }
        else{ // every iteration but last
            i2c_master_ack(0); // sends ACK = 0 (slave should send another byte)
        }
    }
    i2c_master_stop();
}

void OC_setup(void){ // working!! , B15 looks a little noisy though
    // set B15 and B8 as OC1 and OC2
//    ANSELBbits.ANSB8 = 0;
    ANSELBbits.ANSB15 = 0;
    RPB15Rbits.RPB15R = 0b0101; // OC1
    RPB8Rbits.RPB8R = 0b0101; // OC2

    // standard oc skeleton code
    // set duty cycle initially to be 50%
    T2CONbits.TCKPS = 0b011;        // timer prescaler N = 8
    PR2 = 5999;                     // (PR2+1)N/48MHz --- value must be between 1k and 10k
    TMR2 = 0;                       // set timer2 to 0
    T2CONbits.ON = 1;               // turn on timer2

    // B15 = OC1
    OC1CONbits.OCTSEL = 0;          // set OC1 to use timer2
    OC1CONbits.OCM = 0b110;         // PWM mode without fault pin; other OC1CON bits are defaults
    OC1RS = 3000;                   // duty cycle = OC1RS/(PR2+1) = 50%
    OC1R = 3000;                    // OC1R for just in case it rolls over
    OC1CONbits.ON = 1;              // turn on OC1

    // B13 = OC2
    OC2CONbits.OCTSEL = 0;          // set OC2 to use timer2
    OC2CONbits.OCM = 0b110;         // PWM mode without fault pin; other OC1CON bits are defaults
    OC2RS = 3000;                   // duty cycle = OC1RS/(PR2+1) = 50%
    OC2R = 3000;                    // OC2R for just in case it rolls over
    OC2CONbits.ON = 1;              // turn on OC2
}

//// get the current logic of the expander
//unsigned char getExpander(void) { // working properly?
//    i2c_master_start();
//    i2c_master_send(DEV);
//    i2c_master_send(IMU_ADDR; // read from the GPIO register to get logic
//    i2c_master_restart();
//    i2c_master_send(0b01000001); // send the read command, 1 lsb means read
//    unsigned char r = i2c_master_recv();
//    i2c_master_ack(1);
//    i2c_master_stop();
//    return r;
//}