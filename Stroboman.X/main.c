#include <xc.h>

// CONFIG1L
#pragma config CPUDIV = NOCLKDIV// CPU System Clock Selection bits (No CPU System Clock divide)
#pragma config USBDIV = OFF     // USB Clock Selection bit (USB clock comes directly from the OSC1/OSC2 oscillator block; no divide)

// CONFIG1H
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config PLLEN = ON       // 4 X PLL Enable bit (Oscillator multiplied by 4)
#pragma config PCLKEN = ON      // Primary Clock Enable bit (Primary clock enabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor disabled)
#pragma config IESO = OFF       // Internal/External Oscillator Switchover bit (Oscillator Switchover mode disabled)

// CONFIG2L
#pragma config PWRTEN = ON      // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOREN = SBORDIS  // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware only (SBOREN is disabled))
#pragma config BORV = 19        // Brown-out Reset Voltage bits (VBOR set to 1.9 V nominal)

// CONFIG2H
#pragma config WDTEN = OFF      // Watchdog Timer Enable bit (WDT is controlled by SWDTEN bit of the WDTCON register)
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)

// CONFIG3H
#pragma config HFOFST = OFF     // HFINTOSC Fast Start-up bit (The system clock is held off until the HFINTOSC is stable.)
#pragma config MCLRE = OFF      // MCLR Pin Enable bit (RA3 input pin enabled; MCLR disabled)

// CONFIG4L
#pragma config STVREN = ON      // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF        // Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
#pragma config BBSIZ = OFF      // Boot Block Size Select bit (1kW boot block size)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled (Legacy mode))

// CONFIG5L
#pragma config CP0 = OFF        // Code Protection bit (Block 0 not code-protected)
#pragma config CP1 = OFF        // Code Protection bit (Block 1 not code-protected)

// CONFIG5H
#pragma config CPB = OFF        // Boot Block Code Protection bit (Boot block not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protection bit (Data EEPROM not code-protected)

// CONFIG6L
#pragma config WRT0 = OFF       // Table Write Protection bit (Block 0 not write-protected)
#pragma config WRT1 = OFF       // Table Write Protection bit (Block 1 not write-protected)

// CONFIG6H
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot block not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not write-protected)

// CONFIG7L
#pragma config EBTR0 = OFF      // Table Read Protection bit (Block 0 not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Table Read Protection bit (Block 1 not protected from table reads executed in other blocks)

// CONFIG7H
#pragma config EBTRB = OFF      // Boot Block Table Read Protection bit (Boot block not protected from table reads executed in other blocks)


#include <stdint.h>
#include <delays.h>


//
// DISPLAY
// -----------------
// LCD 1=LED 2=GND 3=VCC 4=CLOCK 5=DATA 6=CS 7=BUT-UP 8=BUT-OK 9=BUT-DO
//
// PIC18F14K50
// ------------------
//     VDD  1  20 GND
//     OSC1 2  19 RA0/D+
//     OSC2 3  18 RA1/D-
//     MCLR 4  17 VUSB
//     RC5  5  16 RC0/INT0
//     RC4  6  15 RC1/INT1
//     RC3  7  14 RC2/INT2
//     RC6  8  13 RB4
//     RC7  9  12 RB5/RX
//  TX/RB7 10  11 RB6
//
// RB6 -> LCD_CLOCK
// RC6 -> LCD_CS
// RC7 -> LCD_DATA
//

#define TACH_TRIS       TRISCbits.TRISC1
#define TACH            LATCbits.LATC1


#define LED_TRIS        TRISCbits.TRISC0
#define LED             LATCbits.LATC0

#define LCD_C           0         // Command mode for LCD
#define LCD_D           1         // Data mode LCD

#define SPI_CLOCK_TRIS  TRISBbits.TRISB6
#define SPI_CLOCK       LATBbits.LATB6
#define SPI_CS_TRIS     TRISCbits.TRISC6
#define SPI_CS          LATCbits.LATC6
#define SPI_DATA_TRIS   TRISCbits.TRISC7
#define SPI_DATA        LATCbits.LATC7

#define LCD_CE_LOW      SPI_CS=0
#define LCD_CE_HIGH     SPI_CS=1

#define LCD_CLK_LOW     SPI_CLOCK=0
#define LCD_CLK_HIGH    SPI_CLOCK=1

#define LCD_DI_LOW      SPI_DATA=0
#define LCD_DI_HIGH     SPI_DATA=1


typedef struct {
    uint8_t lowestByte;
    uint8_t lowByte;
    uint8_t highByte;
    uint8_t highestByte;
} t_4bytes;

typedef union {
    t_4bytes u8;
    uint32_t u32;
  } t_4bytes32;


static volatile uint32_t tach;

static volatile uint8_t tach_overflow;
static volatile uint8_t newTachData;
static volatile t_4bytes32 tachData;

static volatile uint16_t delay;


uint8_t SpiSend(uint8_t data);
void LcdInit(void);
void LcdXY(uint8_t x, uint8_t y);
void LcdClear(void);
void LcdCharacter(char ch);
void LcdTinyDigit(char ch);
void LcdString(char *string);
void LcdPrintUint16(uint16_t value, uint8_t type);



void SpiSetup() {
    // Set SPI ports to output
    SPI_CS_TRIS=0;
    SPI_DATA_TRIS=0;
    SPI_CLOCK_TRIS=0;
    LCD_CE_HIGH;

    SSPCON1 = 0b00000001;       // 
    SSPSTATbits.SMP = 0;        //
    SSPSTATbits.CKE = 1;        // transmit data on rising edge of clock
    SSPCON1bits.SSPEN=0;        // Disable SPI
}






//
//
//
uint8_t SpiSend(uint8_t data) {
    SSPBUF=data;
    while(!SSPSTATbits.BF);
    return SSPBUF;
}


//
// Send a byte to the display, either as a command or data depending on the "cd" flag
//
void LcdSend(uint8_t cd, uint8_t data) {
    LCD_CE_LOW;                                   // Enable LCD ~CE
    if (cd==0) LCD_DI_LOW; else LCD_DI_HIGH;      // Command/Data-bit
    NOP();
    LCD_CLK_HIGH;
    NOP();
    LCD_CLK_LOW;
    SSPCON1bits.SSPEN=1;        // Enable SPI
    SpiSend(data);
    SSPCON1bits.SSPEN=0;        // Disable SPI
    LCD_CE_HIGH;                                  // Disable LCD ~CE
}




//
// Set the cursor position for LcdCharacter()/LcdString()
// The x coordinate is in pixels, the y coordinate is the line
// ranging from 0 to 7
//
void LcdXY(uint8_t x, uint8_t y) {
  LcdSend(LCD_C,0xB0 | (y & 0x0F));
  LcdSend(LCD_C,0x10 | ((x >> 4) & 0x07));
  LcdSend(LCD_C,0x00 | (x & 0x0F));
}



//
//
//
void LcdClear(void){
  uint16_t i;
  LcdXY(0,0);
  for (i=0; i<96*9 ; i++) {
    LcdSend(LCD_D, 0x00);
  }
}



//
// Send all the required initialization commands to the display
//
void LcdInit(void) {
    LcdSend(LCD_C, 0xE2);  // Software Reset
    Delay1KTCYx(100);
    LcdSend(LCD_C, 0x3D);  // Charge pump ON
    LcdSend(LCD_C, 0x01);  // Charge pump=4
    LcdSend(LCD_C, 0xA4);  // Display all points = OFF
    LcdSend(LCD_C, 0x2F);  // Booster=ON, Voltage Reg=ON, Voltage Follower=ON
    LcdSend(LCD_C, 0xAF);  // Display ON
    LcdSend(LCD_C, 0xA6);  // Normal display
    LcdSend(LCD_C, 0xC8);  // Normal C0/C8 screen up/down
    LcdSend(LCD_C, 0xA1);  // Normal A0/A1 screen left/right

    //LcdClear();
}


const  char Tahoma15x24[] = {
	0xE0, 0xFF, 0x00, 0xF8, 0xFF, 0x03, 0xFE, 0xFF, 0x0F, 0x1E, 0x00, 0x0F, 0x0F, 0x00, 0x1E, 0x07, 0x00, 0x1C, 0x07, 0x00, 0x1C, 0x07, 0x00, 0x1C, 0x0F, 0x00, 0x1E, 0x1E, 0x00, 0x0F, 0xFE, 0xFF, 0x0F, 0xF8, 0xFF, 0x03, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 0
        0x00, 0x00, 0x00, 0x30, 0x00, 0x1C, 0x38, 0x00, 0x1C, 0x38, 0x00, 0x1C, 0x3C, 0x00, 0x1C, 0xFE, 0xFF, 0x1F, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x1F, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 1
        0x0E, 0x00, 0x1E, 0x0E, 0x00, 0x1F, 0x07, 0x80, 0x1F, 0x07, 0xC0, 0x1D, 0x07, 0xE0, 0x1C, 0x07, 0x70, 0x1C, 0x07, 0x38, 0x1C, 0x07, 0x1C, 0x1C, 0x0F, 0x0F, 0x1C, 0xFE, 0x07, 0x1C, 0xFC, 0x03, 0x1C, 0xF8, 0x00, 0x1C, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 2
        0x00, 0x00, 0x0E, 0x0E, 0x00, 0x0E, 0x0E, 0x00, 0x1C, 0x07, 0x00, 0x1C, 0x07, 0x0E, 0x1C, 0x07, 0x0E, 0x1C, 0x07, 0x0E, 0x1C, 0x07, 0x0E, 0x1C, 0x07, 0x0F, 0x1E, 0x8F, 0x1D, 0x0F, 0xFE, 0xFD, 0x0F, 0xFE, 0xF8, 0x07, 0x78, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 3
        0x00, 0xC0, 0x00, 0x00, 0xF0, 0x00, 0x00, 0xFC, 0x00, 0x00, 0xEF, 0x00, 0xC0, 0xE3, 0x00, 0xF0, 0xE0, 0x00, 0x3C, 0xE0, 0x00, 0x0E, 0xE0, 0x00, 0x03, 0xE0, 0x00, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x1F, 0x00, 0xE0, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00,  // Code for char 4
        0x00, 0x00, 0x0E, 0xFF, 0x07, 0x0E, 0xFF, 0x07, 0x1C, 0xFF, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x0F, 0x1E, 0x07, 0x0E, 0x0F, 0x07, 0xFE, 0x0F, 0x07, 0xFC, 0x07, 0x07, 0xF8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 5
};


const  char Tahoma15x24_5[] = {
         0x80, 0xFF, 0x00, 0xE0, 0xFF, 0x03, 0xF8, 0xFF, 0x07, 0x7C, 0x0E, 0x0F, 0x1E, 0x06, 0x1E, 0x0E, 0x07, 0x1C, 0x0F, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x0F, 0x1E, 0x07, 0x0E, 0x0F, 0x07, 0xFE, 0x0F, 0x00, 0xFC, 0x07, 0x00, 0xF8, 0x01, 0x00, 0x00, 0x00,  // Code for char 6
         0x07, 0x00, 0x00, 0x07, 0x00, 0x10, 0x07, 0x00, 0x1C, 0x07, 0x00, 0x1F, 0x07, 0xC0, 0x0F, 0x07, 0xF0, 0x03, 0x07, 0xFC, 0x00, 0x07, 0x3F, 0x00, 0x87, 0x0F, 0x00, 0xE7, 0x03, 0x00, 0xFF, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Code for char 7
         0x00, 0xE0, 0x01, 0x78, 0xF8, 0x07, 0xFC, 0xF9, 0x0F, 0xFE, 0x0D, 0x0F, 0x8F, 0x07, 0x1E, 0x07, 0x07, 0x1C, 0x07, 0x07, 0x1C, 0x07, 0x06, 0x1C, 0x07, 0x0E, 0x1C, 0x0F, 0x0F, 0x1E, 0xFE, 0x1F, 0x0E, 0xFC, 0xF9, 0x0F, 0x78, 0xF8, 0x07, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x00,  // Code for char 8
         0xF0, 0x03, 0x00, 0xFC, 0x07, 0x00, 0xFE, 0x0F, 0x1C, 0x1E, 0x0E, 0x1C, 0x0F, 0x1E, 0x1C, 0x07, 0x1C, 0x1C, 0x07, 0x1C, 0x1C, 0x07, 0x1C, 0x1E, 0x07, 0x1C, 0x0E, 0x0F, 0x0C, 0x0F, 0x1E, 0xCE, 0x07, 0xFC, 0xFF, 0x03, 0xF8, 0xFF, 0x00, 0xE0, 0x3F, 0x00, 0x00, 0x00, 0x00,  // Code for char 9
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x1E, 0xE0, 0x01, 0x1E, 0xE0, 0x01, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Code for char :
};


const char font6_8[][6] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // sp
    { 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00 },   // !
    { 0x00, 0x00, 0x07, 0x00, 0x07, 0x00 },   // "
    { 0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14 },   // #
    { 0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   // $
    { 0x00, 0x62, 0x64, 0x08, 0x13, 0x23 },   // %
    { 0x00, 0x36, 0x49, 0x55, 0x22, 0x50 },   // &
    { 0x00, 0x00, 0x05, 0x03, 0x00, 0x00 },   // '
    { 0x00, 0x00, 0x1c, 0x22, 0x41, 0x00 },   // (
    { 0x00, 0x00, 0x41, 0x22, 0x1c, 0x00 },   // )
    { 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14 },   // *
    { 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08 },   // +
    { 0x00, 0x00, 0x00, 0xA0, 0x60, 0x00 },   // ,
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08 },   // -
    { 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },   // .
    { 0x00, 0x20, 0x10, 0x08, 0x04, 0x02 },   // /
    { 0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E },   // 0
    { 0x00, 0x00, 0x42, 0x7F, 0x40, 0x00 },   // 1
    { 0x00, 0x42, 0x61, 0x51, 0x49, 0x46 },   // 2
    { 0x00, 0x21, 0x41, 0x45, 0x4B, 0x31 },   // 3
    { 0x00, 0x18, 0x14, 0x12, 0x7F, 0x10 },   // 4
    { 0x00, 0x27, 0x45, 0x45, 0x45, 0x39 },   // 5
    { 0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30 },   // 6
    { 0x00, 0x01, 0x71, 0x09, 0x05, 0x03 },   // 7
    { 0x00, 0x36, 0x49, 0x49, 0x49, 0x36 },   // 8
    { 0x00, 0x06, 0x49, 0x49, 0x29, 0x1E },   // 9
    { 0x00, 0x00, 0x36, 0x36, 0x00, 0x00 },   // :
    { 0x00, 0x00, 0x56, 0x36, 0x00, 0x00 },   // ;
    { 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },   // <
    { 0x00, 0x14, 0x14, 0x14, 0x14, 0x14 },   // =
    { 0x00, 0x00, 0x41, 0x22, 0x14, 0x08 },   // >
    { 0x00, 0x02, 0x01, 0x51, 0x09, 0x06 },   // ?
    { 0x00, 0x32, 0x49, 0x59, 0x51, 0x3E },   // @
    { 0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C },   // A
    { 0x00, 0x7F, 0x49, 0x49, 0x49, 0x36 },   // B
    { 0x00, 0x3E, 0x41, 0x41, 0x41, 0x22 },   // C
    { 0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C },   // D
    { 0x00, 0x7F, 0x49, 0x49, 0x49, 0x41 },   // E
    { 0x00, 0x7F, 0x09, 0x09, 0x09, 0x01 },   // F
    { 0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A },   // G
    { 0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F },   // H
    { 0x00, 0x00, 0x41, 0x7F, 0x41, 0x00 },   // I
    { 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01 },   // J
    { 0x00, 0x7F, 0x08, 0x14, 0x22, 0x41 },   // K
    { 0x00, 0x7F, 0x40, 0x40, 0x40, 0x40 },   // L
    { 0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F },   // M
    { 0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F },   // N
    { 0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E },   // O
    { 0x00, 0x7F, 0x09, 0x09, 0x09, 0x06 },   // P
    { 0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E },   // Q
    { 0x00, 0x7F, 0x09, 0x19, 0x29, 0x46 },   // R
    { 0x00, 0x46, 0x49, 0x49, 0x49, 0x31 },   // S
    { 0x00, 0x01, 0x01, 0x7F, 0x01, 0x01 },   // T
    { 0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F },   // U
    { 0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F },   // V
    { 0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F },   // W
    { 0x00, 0x63, 0x14, 0x08, 0x14, 0x63 },   // X
    { 0x00, 0x07, 0x08, 0x70, 0x08, 0x07 },   // Y
    { 0x00, 0x61, 0x51, 0x49, 0x45, 0x43 },   // Z
    { 0x00, 0x00, 0x7F, 0x41, 0x41, 0x00 },   // [
    { 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x55 },   // 55
    { 0x00, 0x00, 0x41, 0x41, 0x7F, 0x00 },   // ]
    { 0x00, 0x04, 0x02, 0x01, 0x02, 0x04 },   // ^
    { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40 },   // _
    { 0x00, 0x00, 0x01, 0x02, 0x04, 0x00 },   // '
    { 0x00, 0x20, 0x54, 0x54, 0x54, 0x78 },   // a
    { 0x00, 0x7F, 0x48, 0x44, 0x44, 0x38 },   // b
    { 0x00, 0x38, 0x44, 0x44, 0x44, 0x20 },   // c
    { 0x00, 0x38, 0x44, 0x44, 0x48, 0x7F },   // d
    { 0x00, 0x38, 0x54, 0x54, 0x54, 0x18 },   // e
    { 0x00, 0x08, 0x7E, 0x09, 0x01, 0x02 },   // f
    { 0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C },   // g
    { 0x00, 0x7F, 0x08, 0x04, 0x04, 0x78 },   // h
    { 0x00, 0x00, 0x44, 0x7D, 0x40, 0x00 },   // i
    { 0x00, 0x40, 0x80, 0x84, 0x7D, 0x00 },   // j
    { 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00 },   // k
    { 0x00, 0x00, 0x41, 0x7F, 0x40, 0x00 },   // l
    { 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78 },   // m
    { 0x00, 0x7C, 0x08, 0x04, 0x04, 0x78 },   // n
    { 0x00, 0x38, 0x44, 0x44, 0x44, 0x38 },   // o
    { 0x00, 0xFC, 0x24, 0x24, 0x24, 0x18 },   // p
    { 0x00, 0x18, 0x24, 0x24, 0x18, 0xFC },   // q
    { 0x00, 0x7C, 0x08, 0x04, 0x04, 0x08 },   // r
    { 0x00, 0x48, 0x54, 0x54, 0x54, 0x20 },   // s
    { 0x00, 0x04, 0x3F, 0x44, 0x40, 0x20 },   // t
    { 0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C },   // u
    { 0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C },   // v
    { 0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C },   // w
    { 0x00, 0x44, 0x28, 0x10, 0x28, 0x44 },   // x
    { 0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C },   // y
    { 0x00, 0x44, 0x64, 0x54, 0x4C, 0x44 },   // z
    { 0x00,0x00, 0x06, 0x09, 0x09, 0x06 }    // horiz lines
};




void LCD_BIG_CHAR(unsigned char row, unsigned char col, unsigned char chr){

    const char* f0;
    const char* f;
    unsigned char r, c, y;
	unsigned char ch;
   y = (chr & 0x0F);

	if (y < 6)
	f0 = Tahoma15x24 + y* 45;
		else
		f0 = Tahoma15x24_5 + (y - 6 )* 45;

    for(r = 0; r<3; r++)
    {
        f = f0 + r;
       LcdXY (col, (row +r));
        for(c = 0; c<14; c++)
        {
            ch = *f;
            LcdSend(LCD_D,ch);
            f=f + 3;
        }
    }
}


#define LED25uS     19
#define LED50uS     38
#define LED75uS     56
#define LED100uS    75
#define LED125uS    94
#define LED150uS    113
#define LED175uS    131
#define LED200uS    150
#define LED225uS    169
#define LED250uS    188
#define LED275uS    206
#define LED300uS    225
#define LED325uS    244
#define LED340uS    256



void interrupt ISR() {
    static uint8_t rev=0;
    static uint8_t led=0;

    // Timer0 is used to count the RPM of the motor
    // At any decent speed it will overflow multiple times so
    // we need to keep track of the number of overflows.
    if (TMR0IF) {
        tach_overflow++;
            delay++;
        TMR0IF=0;       // Clear Timer0 interrupt flag
    }

    // HW Interrupt1 is connected to the tachometer to measure the 
    // speed of the motor.
    if (INT1IF) {
        if ((rev++) & 0x01) {
            // We have a full revolution of the motor, so grab the
            // values from Timer0 plus its overflow counter and stick
            // them into a variable for the RPM display routine in the
            // main function.
            tachData.u8.lowestByte=TMR0L;
            tachData.u8.lowByte=TMR0H;
            TMR0H=0;
            TMR0L=0;
            tachData.u8.highByte=tach_overflow;
            tach_overflow=0;
            tachData.u8.highestByte=0;
            // Tell the RPM calculator that new measurement data is availabe
            newTachData=1;

        LED=1;
        // Start the timer for turning off the LED
        TMR2=0;             // The timer should start from zero
        TMR2IF=0;           // Clear Timer2 interupt flag
        TMR2IE=1;           // Enable Timer2 interrupts
            
//            
//            // Start delaying to turn on the LED
//            TMR1H = delay>>8;    // preset for timer1 MSB register
//            TMR1L = delay&0xFF;   // preset for timer1 LSB register
//            TMR1IF=0;       // Clear Timer1 interrupt flag
//            TMR1ON = 1;    // bit 0 = Enable Timer1 that turns on the LED
         }
        INT1IF=0;       // Clear HW Interrupt 1 flag
    }

//    // Timer1 is used to turn on the LED after the desired delay
//    if (TMR1IF) {
//        // The turn on timer is a one-shot
//        TMR1ON = 0;
//        // Light up the led for the desired duration
//        LED=1;
//        // Start the timer for turning off the LED
//        TMR2=0;             // The timer should start from zero
//        TMR2IF=0;           // Clear Timer2 interupt flag
//        TMR2IE=1;           // Enable Timer2 interrupts
//
//        TMR1IF=0;       // Clear Timer1 interrupt flag
//    }

    // Timer2 is used to turn off the LED after the desired ON-time
    if (TMR2IF) {
        LED=0;
        TMR2IE=0;       // Disable Timer2 now the LED is turned off
        TMR2IF=0;       // Clear Timer2 interrupt flag
  }
}

#define AVGSIZE 32

//
//
//
void main(void) {

    TRISA=0xFF;     // All inputs
    TRISB=0xFF;     // All inputs
    TRISC=0xFF;     // All inputs
    ANSELH=0x00;    // Digital I/O
    ANSEL=0x00;     // Digital I/O

    LED_TRIS=0;
    LED=1;
    SpiSetup();
    LcdInit();
    LcdClear();
    LCD_BIG_CHAR(0, 0, 5);
    LED=0;

    IPEN=0;

    T0CS=0;	// Use clock from FCPU/4
    PSA=1;	// Don't use prescaler
    T08BIT=0;	// 16 bit mode
    TMR0ON=1;	// Start TIMER0
    TMR0IE=1;	// Enable TIMER0 Interrupt

//    //Timer1 Registers Prescaler= 1 - TMR1 Preset = 12662 - Freq = 0.23 Hz - Period = 4.406167 seconds
//    T1CKPS1 = 0;   // bits 5-4  Prescaler Rate Select bits
//    T1CKPS0 = 0;   // bit 4
//    T1OSCEN = 1;   // bit 3 Timer1 Oscillator Enable Control bit 1 = on
//    T1SYNC = 1;    // bit 2 Timer1 External Clock Input Synchronization Control bit...1 = Do not synchronize external clock input
//    TMR1CS = 0;    // bit 1 Timer1 Clock Source Select bit...0 = Internal clock (FOSC/4)
//    TMR1ON = 0;    // bit 0 = Disable timer
//    TMR1IE=1;	   // Enable TIMER1 Interrupt
//    delay=49*256+118;
//    TMR1H = delay>>8;    // preset for timer1 MSB register
//    TMR1L = delay&0xFF;   // preset for timer1 LSB register



//    TMR3CS=0;   // Use clock from FCPU/4
//    T3CKPS0=0;  // Prescale 1:1
//    T3CKPS1=0;  // Prescale 1:1
//    TMR3ON=1;	// Start TIMER0
//    TMR3IE=1;	// Enable TIMER0 Interrupt


    //
    //http://eng-serve.com/pic/pic_timer.html
    //
    //Timer2 Registers Prescaler= 16 - TMR2 PostScaler = 1 - PR2 = 75 - Freq = 0.01 Hz - Period = 100.000000 seconds
    T2CON = 0;        // bits 6-3 Post scaler 1:1 thru 1:16
    TMR2ON = 1;  // bit 2 turn timer2 on;
    T2CKPS1 = 1; // bits 1-0  Prescaler Rate Select bits
    T2CKPS0 = 0;
    PR2=LED125uS;         // PR2 (Timer2 Match value)
    TMR2IF = 0;            // clear timer1 interupt flag TMR1IF
    TMR2IE = 0;         // disable Timer2 interrupts

    INT1IE=1;   // Enable HW INT1 Interrupts
    INT1IF=0;

    PEIE=1;	// Enable Peripheral Interrupt
    GIE=1;	// Enable INTs globally


    uint32_t rpm,rpmTotal;
    uint16_t rpmAvgArr[AVGSIZE];
    uint8_t avgPtr=0;

    for (;;) {

        // If new readings are ready then process them
        if (newTachData) {
            newTachData=0;
            rpmAvgArr[avgPtr]=(uint16_t)(1000.0/(((float)tachData.u32)/(12000.0*60.0)));
            avgPtr++;
            if (avgPtr>=AVGSIZE) avgPtr=0;
            rpmTotal=0;
            for (uint8_t i=0; i<AVGSIZE; i++) rpmTotal+=rpmAvgArr[i];
            rpm=rpmTotal/AVGSIZE;
            LCD_BIG_CHAR(0, 0*14, (rpm/10000)%10);
            LCD_BIG_CHAR(0, 1*14, (rpm/1000)%10);
            LCD_BIG_CHAR(0, 2*14, (rpm/100)%10);
            LCD_BIG_CHAR(0, 3*14, (rpm/10)%10);
            LCD_BIG_CHAR(0, 4*14, (rpm/1)%10);

            LCD_BIG_CHAR(20, 0*14, (avgPtr/100)%10);
            LCD_BIG_CHAR(20, 1*14, (avgPtr/10)%10);
            LCD_BIG_CHAR(20, 2*14, (avgPtr/1)%10);
        }

    }
}
