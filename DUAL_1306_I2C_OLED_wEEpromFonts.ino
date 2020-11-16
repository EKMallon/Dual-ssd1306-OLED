// Adding Dual I2C OLED displays to your Arduino datalogger (w SSD1306) 
// This program is simply an exploraton of screen use/concepts for the Cave Pearl Project
// https://thecavepearlproject.org/how-to-build-an-arduino-data-logger/
// The run-once functions move font(s) & bitmap from progmem into eeprom storage.
// This saves ~2k of program space with the trade-off that it slows display response speed.

// WriteString > WriteCharacter functions based on Julian Ilett's SPI homebrew code for the Nokia 5110 LCD
// see https://www.youtube.com/watch?v=RAlZ1DHw03g&list=PLjzGSu1yGFjXWp5F4BPJg4ZJFbJcWeRzk
// I have tweaked those to work with I2C displays & added a two-pass method for printing large #'s

// Graphing functions are by David Johnson-Davies
// see: Tiny Function Plotter:  http://www.technoblogy.com/show?2CFT
// I have tweaked them slightly to provide dashed background lines & produce bar-graph output

// The Progress-Bar & Display Bitmap functions written by Edward Mallon
// code assumes you have two identical 0.96" I2C screens connected at 0x3C & 0x3D 

// NOTE: runONCE_addData2EEproms should be defined ONLY the FIRST TIME you run the program
// after that ONE TIME comment out #define runONCE_addData2EEproms ->those functions are no longer needed:
// because the bitmaps have already embedded in the 328p internal & AT24c32 eeprom memory

#define runONCE_addData2EEproms   

#include <Wire.h>
#include <EEPROM.h>
char tmp[14];
char charbuffer='A';

int x0;
int y0;
bool oledInvertText = false;
int ssd1306_address = 0x3C;  // 0x3C 1st screen or 0x3D for second screen

// Constant definitions
#define ssd1306_oneCommand              0x80
#define ssd1306_commandStream           0x00 // command stream: C0=0 D/C#=0, followed by 6 zeros (datasheet 8.1.5.2)
#define ssd1306_oneData                 0xC0
#define ssd1306_dataStream              0x40 // data stream: C0 = 0 D/C#=1, followed by 6 zeros

//screen dimensional settings           (for 0.96" ssd1306)
#define ssd1306_PIXEL_WIDTH             128
#define ssd1306_PIXEL_HEIGHT            64
#define ssd1306_PAGE_COUNT              8
#define ssd1306_PAGE_HEIGHT             ssd1306_PIXEL_HEIGHT / ssd1306_PAGE_COUNT
#define ssd1306_SEGMENT_COUNT           ssd1306_PIXEL_WIDTH * ssd1306_PAGE_HEIGHT
#define ssd1306_FONT_WIDTH              5 //will change with other fonts

//** fundamental commands
#define ssd1306_SET_CONTRAST            0b10000001 // 0x81
#define ssd1306_CONTRAST_DEFAULT        0b01111111
#define ssd1306_DISPLAY                 0b10100100
#define ssd1306_DISPLAY_RESET           ssd1306_DISPLAY
#define ssd1306_DISPLAY_ALLON           ssd1306_DISPLAY | 0b01
#define ssd1306_DISPLAY_NORMAL          ssd1306_DISPLAY | 0b10
#define ssd1306_DISPLAY_INVERSE         ssd1306_DISPLAY | 0b11
#define ssd1306_DISPLAY_SLEEP           ssd1306_DISPLAY | 0b1110
#define ssd1306_DISPLAY_ON              ssd1306_DISPLAY | 0b1111
#define ssd1306_DISPLAYALLON_RESUME     0xA4

//** addressing
#define ssd1306_SET_ADDRESSING          0b00100000 // 0x20
#define ssd1306_ADDRESSING_HORIZONTAL   0b00000000
#define ssd1306_ADDRESSING_VERTICAL     0b00000001
#define ssd1306_SET_COLUMN_RANGE        0b00100001 // 0x21
#define ssd1306_SET_PAGE_RANGE          0b00100010 // 0x22

#define ssd1306_ADDRESSING_PAGE           0b10
/**Set GDDRAM Page Start Address. */
#define ssd1306_PageMode_SETSTARTPAGEmode 0xB0
/** Set Lower Column Start Address for Page Addressing Mode. */
#define ssd1306_PageMode_SETLOWCOLUMN     0x00
/** Set Higher Column Start Address for Page Addressing Mode. */
#define ssd1306_PageMode_SETHIGHCOLUMN    0x10

//** hardware config
#define ssd1306_SET_START_LINE          0b01000000 // 0x40
#define ssd1306_START_LINE_DEFAULT      0b00000000
#define ssd1306_SET_SEG_SCAN            0b10100000 // 0xA0
#define ssd1306_SET_SEG_SCAN_DEFAULT    ssd1306_SET_SEG_SCAN | 0b00
#define ssd1306_SET_SEG_SCAN_REVERSE    ssd1306_SET_SEG_SCAN | 0b01
#define ssd1306_SET_MULTIPLEX_RATIO     0b10101000 // 0xA8
#define ssd1306_MULTIPLEX_RATIO_DEFAULT 0b00111111
#define ssd1306_SET_COM_SCAN            0b11000000 // 0xC0
#define ssd1306_SET_COM_SCAN_DEFAULT    ssd1306_SET_COM_SCAN | 0b0000
#define ssd1306_SET_COM_SCAN_REVERSE    ssd1306_SET_COM_SCAN | 0b1000
#define ssd1306_SET_DISPLAY_OFFSET      0b11010011 // 0xD3
#define ssd1306_DISPLAY_OFFSET_DEFAULT  0b00000000
#define ssd1306_SET_COM_PINS            0b11011010 // 0xDA
#define ssd1306_COM_PINS_DEFAULT        0b00010010

//** timing and driving
#define ssd1306_SET_CLOCK_FREQUENCY     0b11010101 // 0xD5
#define ssd1306_CLOCK_FREQUENCY_DEFAULT 0b10000000
#define ssd1306_SET_PRECHARGE           0b11011001 // 0xD9
#define ssd1306_PRECHARGE_DEFAULT       0b00100010
#define ssd1306_SET_VCOMH_DESELECT      0b11011011 // 0xDB
#define ssd1306_VCOMH_DESELECT_DEFAULT  0b00100000
#define ssd1306_SET_CHARGE_PUMP         0b10001101 // 0x8D
#define ssd1306_CHARGE_PUMP_ENABLE      0b00010100
#define ssd1306_NOP 0xE3

// Display initialization sequence
const uint8_t ssd1306_init_sequence [] PROGMEM = {
  ssd1306_SET_MULTIPLEX_RATIO,              //0b10101000 = 0xA8
  ssd1306_MULTIPLEX_RATIO_DEFAULT,          //0b00111111  =0x3F 
  //0x3F is default for 128x64 (64-1) but this is different for other screen aspect ratios 
  ssd1306_DISPLAY_RESET,                    //0xA4 =Normal display mode
  ssd1306_SET_DISPLAY_OFFSET,               //0b11010011 // 0xD3 
  ssd1306_DISPLAY_OFFSET_DEFAULT,           //0b00000000
  (ssd1306_SET_START_LINE | ssd1306_START_LINE_DEFAULT),//0b01000000 =0x40

  // reverse both to match byte order of LCD assistant // most libraries like Adafruit do the same
  ssd1306_SET_SEG_SCAN_REVERSE,             // 0xA1 Scan from COM7 to COM0= Flip horizontally //default = A0h = (Set Segment Re-map) (A0h / A1h)
  ssd1306_SET_COM_SCAN_REVERSE,             // COMSCANDEC = 0xc8 = Vertical mirroring  //default = COMSCANINC = 0xc0
  // REVERSING both SEG & COM rotates & flips the image so 0,0 is upper left corner
  // see: https://electronics.stackexchange.com/questions/273768/flip-the-image-of-oled-128x64-0-96-inch-display
  
  ssd1306_SET_COM_PINS,                     //0xDA
  ssd1306_COM_PINS_DEFAULT,                 //0x12
  ssd1306_SET_CONTRAST,                     //0x81(2-byte)
  ssd1306_CONTRAST_DEFAULT,                 //0x7F  
                                            //0x7F default draws about 20% less current than full contrast  0xFF
  ssd1306_DISPLAYALLON_RESUME,              //0xa4, disable entire display on
  ssd1306_DISPLAY_NORMAL,                   //0xa6, //set normal display
  ssd1306_SET_CLOCK_FREQUENCY,              //0xD5  //FOSC is the oscillator frequency.
  ssd1306_CLOCK_FREQUENCY_DEFAULT,0b11110010, //(4Bit: Fosc) (4Bit: Clkdiv) //lowering clkdiv reduces current a bit
  //freq is left-most 4 bits 0000=slowest ~300khz,  1000 = default = middle ~400khz, 1111=fastest ~550khz oscilator freq
  //see https://community.arduboy.com/t/unsolvable-vsync-issues-and-screen-tearing/140/20
  ssd1306_SET_CHARGE_PUMP,                  //0b10001101 = 0x8D
  ssd1306_CHARGE_PUMP_ENABLE,               //0b00010100 = 0x14 =use internal VCC  //0x10=external vcc
 
  //if (vccstate == ssd1306_EXTERNALVCC) 
  //{ ssd1306_command(0x9F); } else { ssd1306_command(0xCF); }

  ssd1306_SET_PRECHARGE,                    //0b11011001 = 0xD9
  ssd1306_PRECHARGE_DEFAULT,                //0b00100010 =0x22 //default for internal vcc // 0xF1 for EXTERNALVCC
  ssd1306_SET_VCOMH_DESELECT,               //0b11011011 =0xDB  //This command adjusts the VCOMH regulator output.
  ssd1306_VCOMH_DESELECT_DEFAULT,           //0b00100000 =0x20 default  // or 0x40?
  ssd1306_DISPLAY_ON,                       //0b10101111 =0xAF 
  ssd1306_DISPLAY_RESET,                    //may need this later? https://forum.arduino.cc/index.php?topic=541692.0

/* for scrolling: (not used here)
    0x27, //set right horizontal scroll
    0x0,  //dummy byte
    0x0,  //page start address
    0x7,  //scroll speed
    0x7,  //page end address
    0x0,  //dummy byte
    0xff, //dummy byte
    0x2f, //start scrolling
*/
};

void ssd1306_InitScreen(void) {
  Wire.beginTransmission(ssd1306_address);    //d_start_cmd();
  Wire.write(ssd1306_commandStream);          // prep command stream: C0=0 D/C#=0, followed by 6 zeros (datasheet 8.1.5.2)
  for (uint8_t i = 0; i < sizeof (ssd1306_init_sequence); i++) {
    Wire.write(pgm_read_byte(&ssd1306_init_sequence[i]));
  }
  Wire.endTransmission();
}

//==================================================================
#ifdef runONCE_addData2EEproms  //Only needs to happen ONE time
//==================================================================

// the 5x7 fonts: 21 character wide collumns x 8 page/rows fills the display
// Note: ASCII contains the entire upper & lower case font
// but I often store a capital letters only sub-set in the eeprom
// to leave room for sensor calibration constants & logfile header data
const byte ASCII[][5] PROGMEM =
{
 {0x00, 0x00, 0x00, 0x00, 0x00} // 20  
,{0x00, 0x00, 0x5f, 0x00, 0x00} // 21 !
,{0x00, 0x07, 0x00, 0x07, 0x00} // 22 "
,{0x14, 0x7f, 0x14, 0x7f, 0x14} // 23 #
,{0x24, 0x2a, 0x7f, 0x2a, 0x12} // 24 $
,{0x23, 0x13, 0x08, 0x64, 0x62} // 25 %
,{0x36, 0x49, 0x55, 0x22, 0x50} // 26 &
,{0x00, 0x05, 0x03, 0x00, 0x00} // 27 '
,{0x00, 0x1c, 0x22, 0x41, 0x00} // 28 (
,{0x00, 0x41, 0x22, 0x1c, 0x00} // 29 )
,{0x14, 0x08, 0x3e, 0x08, 0x14} // 2a *
,{0x08, 0x08, 0x3e, 0x08, 0x08} // 2b +
,{0x00, 0x50, 0x30, 0x00, 0x00} // 2c ,
,{0x00, 0x08, 0x08, 0x08, 0x08} // 2d -  //ascii decimal 45 //row 13 of array
,{0x00, 0x60, 0x60, 0x00, 0x00} // 2e .
,{0x20, 0x10, 0x08, 0x04, 0x02} // 2f /
,{0x3e, 0x51, 0x49, 0x45, 0x3e} // 30 0
,{0x00, 0x42, 0x7f, 0x40, 0x00} // 31 1
,{0x42, 0x61, 0x51, 0x49, 0x46} // 32 2
,{0x21, 0x41, 0x45, 0x4b, 0x31} // 33 3
,{0x18, 0x14, 0x12, 0x7f, 0x10} // 34 4
,{0x27, 0x45, 0x45, 0x45, 0x39} // 35 5
,{0x3c, 0x4a, 0x49, 0x49, 0x30} // 36 6
,{0x01, 0x71, 0x09, 0x05, 0x03} // 37 7
,{0x36, 0x49, 0x49, 0x49, 0x36} // 38 8
,{0x06, 0x49, 0x49, 0x29, 0x1e} // 39 9
,{0x00, 0x36, 0x36, 0x00, 0x00} // 3a :
,{0b00000000,0b00000110,0b00001001,0b00001001,0b00000110} //redefined as 'degree' symbol // 3b ; altered from original ';' {0x00, 0x56, 0x36, 0x00, 0x00} 
,{0x08, 0x14, 0x22, 0x41, 0x00} // 3c <
,{0x14, 0x14, 0x14, 0x14, 0x14} // 3d =
,{0x00, 0x41, 0x22, 0x14, 0x08} // 3e >
,{0x02, 0x01, 0x51, 0x09, 0x06} // 3f ?
,{0x32, 0x49, 0x79, 0x41, 0x3e} // 40 @
,{0x7e, 0x11, 0x11, 0x11, 0x7e} // 41 A
,{0x7f, 0x49, 0x49, 0x49, 0x36} // 42 B
,{0x3e, 0x41, 0x41, 0x41, 0x22} // 43 C
,{0x7f, 0x41, 0x41, 0x22, 0x1c} // 44 D
,{0x7f, 0x49, 0x49, 0x49, 0x41} // 45 E
,{0x7f, 0x09, 0x09, 0x09, 0x01} // 46 F
,{0x3e, 0x41, 0x49, 0x49, 0x7a} // 47 G
,{0x7f, 0x08, 0x08, 0x08, 0x7f} // 48 H
,{0x00, 0x41, 0x7f, 0x41, 0x00} // 49 I
,{0x20, 0x40, 0x41, 0x3f, 0x01} // 4a J
,{0x7f, 0x08, 0x14, 0x22, 0x41} // 4b K
,{0x7f, 0x40, 0x40, 0x40, 0x40} // 4c L
,{0x7f, 0x02, 0x0c, 0x02, 0x7f} // 4d M
,{0x7f, 0x04, 0x08, 0x10, 0x7f} // 4e N
,{0x3e, 0x41, 0x41, 0x41, 0x3e} // 4f O
,{0x7f, 0x09, 0x09, 0x09, 0x06} // 50 P
,{0x3e, 0x41, 0x51, 0x21, 0x5e} // 51 Q
,{0x7f, 0x09, 0x19, 0x29, 0x46} // 52 R
,{0x46, 0x49, 0x49, 0x49, 0x31} // 53 S
,{0x01, 0x01, 0x7f, 0x01, 0x01} // 54 T
,{0x3f, 0x40, 0x40, 0x40, 0x3f} // 55 U
,{0x1f, 0x20, 0x40, 0x20, 0x1f} // 56 V
,{0x3f, 0x40, 0x38, 0x40, 0x3f} // 57 W
,{0x63, 0x14, 0x08, 0x14, 0x63} // 58 X
,{0x07, 0x08, 0x70, 0x08, 0x07} // 59 Y
,{0x61, 0x51, 0x49, 0x45, 0x43} // 5a Z   //ascii decimal 90 // row 58 of array
,{0x00, 0x7f, 0x41, 0x41, 0x00} // 5b [
,{0x02, 0x04, 0x08, 0x10, 0x20} // 5c ¥   //un-necessary? could be redefined to a more useful symbol?
,{0x00, 0x41, 0x41, 0x7f, 0x00} // 5d ] 
,{0x04, 0x02, 0x01, 0x02, 0x04} // 5e ^   //un-necessary?
,{0x40, 0x40, 0x40, 0x40, 0x40} // 5f _
,{0x00, 0x01, 0x02, 0x04, 0x00} // 60 `   //un-necessary?
,{0x20, 0x54, 0x54, 0x54, 0x78} // 61 a
,{0x7f, 0x48, 0x44, 0x44, 0x38} // 62 b
,{0x38, 0x44, 0x44, 0x44, 0x20} // 63 c
,{0x38, 0x44, 0x44, 0x48, 0x7f} // 64 d
,{0x38, 0x54, 0x54, 0x54, 0x18} // 65 e
,{0x08, 0x7e, 0x09, 0x01, 0x02} // 66 f
,{0x0c, 0x52, 0x52, 0x52, 0x3e} // 67 g
,{0x7f, 0x08, 0x04, 0x04, 0x78} // 68 h
,{0x00, 0x44, 0x7d, 0x40, 0x00} // 69 i
,{0x20, 0x40, 0x44, 0x3d, 0x00} // 6a j 
,{0x7f, 0x10, 0x28, 0x44, 0x00} // 6b k
,{0x00, 0x41, 0x7f, 0x40, 0x00} // 6c l
,{0x7c, 0x04, 0x18, 0x04, 0x78} // 6d m
,{0x7c, 0x08, 0x04, 0x04, 0x78} // 6e n
,{0x38, 0x44, 0x44, 0x44, 0x38} // 6f o
,{0x7c, 0x14, 0x14, 0x14, 0x08} // 70 p
,{0x08, 0x14, 0x14, 0x18, 0x7c} // 71 q
,{0x7c, 0x08, 0x04, 0x04, 0x08} // 72 r
,{0x48, 0x54, 0x54, 0x54, 0x20} // 73 s
,{0x04, 0x3f, 0x44, 0x40, 0x20} // 74 t
,{0x3c, 0x40, 0x40, 0x20, 0x7c} // 75 u
,{0x1c, 0x20, 0x40, 0x20, 0x1c} // 76 v
,{0x3c, 0x40, 0x30, 0x40, 0x3c} // 77 w
,{0x44, 0x28, 0x10, 0x28, 0x44} // 78 x
,{0x0c, 0x50, 0x50, 0x50, 0x3c} // 79 y
,{0x44, 0x64, 0x54, 0x4c, 0x44} // 7a z
,{0x00, 0x08, 0x36, 0x41, 0x00} // 7b {   // un-necessary?
,{0x00, 0x00, 0xFF, 0x00, 0x00} // 7c |   //  EXTENDED by one pixel to build unbroken vertical lines
,{0x00, 0x41, 0x36, 0x08, 0x00} // 7d }   // un-necessary?
,{0x10, 0x08, 0x08, 0x10, 0x08} // 7e ←  // un-necessary?
,{0x78, 0x46, 0x41, 0x46, 0x78} // 7f →  (decimal 127) // un-necessary?
};

// double size: 7 collumns x 3 rows fills the display  //this big# font requires 386 bytes of storage
// for details on how I use this "two-pass" big # font method see: 
// https://thecavepearlproject.org/2018/05/18/adding-the-nokia-5110-lcd-to-your-arduino-data-logger/
const byte Big11x16numberTops[][11] PROGMEM = {
  0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00, // Code for char -
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Code for char .
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Code for char / BUT has been redefined as a "BLANK SPACE"
  0xFE,0xFF,0x03,0x03,0x03,0xC3,0x33,0x0B,0x07,0xFF,0xFE, // Code for char 0
  0x00,0x03,0x03,0x03,0x03,0xFF,0xFF,0x00,0x00,0x00,0x00, // Code for char 1
  0x03,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0xFF,0xFE, // Code for char 2
  0x00,0x03,0x03,0x83,0x83,0x83,0x83,0x83,0x83,0xFF,0xFE, // Code for char 3
  0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0xFC,0xFC,0x00, // Code for char 4
  0x00,0x00,0xFF,0xFF,0x83,0x83,0x83,0x83,0x83,0x83,0x03, // Code for char 5
  0xFF,0xFF,0x03,0x03,0x03,0x03,0x03,0x03,0x07,0x06,0x00, // Code for char 6
  0x07,0x07,0x03,0x03,0x03,0x03,0x83,0x83,0xC3,0x7F,0x7F, // Code for char 7
  0x00,0x80,0xFF,0xFF,0x83,0x83,0x83,0xFF,0xFF,0x80,0x00, // Code for char 8
  0xFF,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0xFF, // Code for char 9
};

const byte Big11x16numberBottoms[][11] PROGMEM = {
  0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00, // Code for char -  = 0x2d ascii
  0x00,0x00,0x00,0x00,0x0E,0x0E,0x0E,0x0E,0x00,0x00,0x00, // Code for char .
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Code for char / IS NOW A "BLANK SPACE" character
  0x3F,0x7F,0x70,0x6C,0x63,0x60,0x60,0x60,0x60,0x7F,0x3F, // Code for char 0
  0x00,0x60,0x60,0x60,0x60,0x7F,0x7F,0x60,0x60,0x60,0x60, // Code for char 1
  0x7F,0x7F,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x60, // Code for char 2
  0x00,0x60,0x60,0x61,0x61,0x61,0x61,0x61,0x61,0x7F,0x3F, // Code for char 3
  0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x7F,0x7F,0x03, // Code for char 4
  0x00,0x38,0x79,0x61,0x61,0x61,0x61,0x61,0x61,0x7F,0x3F, // Code for char 5
  0x7F,0x7F,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x7F,0x7F, // Code for char 6
  0x00,0x00,0x00,0x00,0x00,0x7F,0x7F,0x01,0x00,0x00,0x00, // Code for char 7
  0x3F,0x7F,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x7F,0x3F, // Code for char 8
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x7F, // Code for char 9
};

// bitmap byte order 0.0 is is upper left corner (ie: with com&seg scan reversed)
// to replace this bitmap byte array, use graphic editor to make a 128x64 monochrome image, 
// then convert that with an online Bitmap Converter eg: http://www.majer.ch/lcd/adf_bitmap.php 
const uint8_t backgroundBitmap [] PROGMEM = {    // 128x64 bitmap
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x08, 0x30, 0xE0, 0x00, 0x00, 0x00, 0x20, 0x40,
0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0xC0, 0xC0, 0xE0,
0xE0, 0xE0, 0xE0, 0xF0, 0xA0, 0xF0, 0xF0, 0x68, 0xF0, 0xF0,
0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xF0,
0x8C, 0xF0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xF4, 0x00, 0x00,
0x00, 0x10, 0xFC, 0x10, 0x10, 0x80, 0x00, 0x00, 0x10, 0xFC,
0x10, 0x10, 0x80, 0x00, 0x00, 0x04, 0xFC, 0x00, 0x00, 0x00,
0xE0, 0x50, 0x50, 0x50, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xF0, 0x10, 0xE0, 0x10, 0xE0, 0x00, 0x00, 0xE0, 0x10,
0x10, 0x10, 0xE0, 0x00, 0xF0, 0x20, 0x10, 0x10, 0x20, 0x00,
0x00, 0xE0, 0x50, 0x50, 0x50, 0x60, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x0F, 0x30, 0x00, 0x80, 0x38, 0xDC, 0xF7,
0xF1, 0xCC, 0xC7, 0x6F, 0x1B, 0x83, 0xCF, 0xEF, 0x07, 0x1F,
0x9F, 0x9F, 0x9F, 0xBB, 0x3F, 0x7E, 0x7F, 0x7D, 0x16, 0x05,
0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00,
0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
0x00, 0x01, 0x01, 0x80, 0x80, 0x00, 0x00, 0x00, 0x01, 0x01,
0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x81,
0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
0x80, 0xC0, 0xC0, 0xE0, 0xE0, 0xE0, 0x4E, 0x3F, 0xF3, 0xE7,
0xAF, 0x2F, 0x5E, 0x9D, 0xB9, 0xB9, 0x30, 0x1E, 0xFF, 0xFC,
0x74, 0x76, 0x71, 0x70, 0x30, 0x1C, 0x00, 0x80, 0x06, 0x06,
0x06, 0x04, 0x0C, 0x08, 0x00, 0x10, 0x00, 0x00, 0x40, 0x40,
0x40, 0x00, 0x80, 0x00, 0x3F, 0x08, 0x14, 0x14, 0x22, 0x00,
0x00, 0x3E, 0x04, 0x02, 0x02, 0x3C, 0x00, 0x1C, 0x22, 0x22,
0x22, 0x1C, 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x20, 0x1E, 0x00,
0x00, 0x20, 0x3F, 0x20, 0x00, 0x00, 0x00, 0x1C, 0x2A, 0x2A,
0x2A, 0x2C, 0x00, 0x1C, 0x22, 0x22, 0x14, 0x3F, 0x00, 0x9C,
0xA2, 0xA2, 0x94, 0x7E, 0x00, 0x00, 0x1C, 0x2A, 0x2A, 0x2A,
0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x80, 0x80, 0xE8, 0xFC, 0xFE, 0xFC, 0xF9, 0xE7, 0x8F,
0x3F, 0x7F, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x01, 0x01, 0x03,
0x03, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xF1, 0xFE, 0x7E, 0x3E,
0x9E, 0xCE, 0xDC, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
0x40, 0xD0, 0x00, 0x00, 0x00, 0x80, 0x40, 0x40, 0x80, 0xC0,
0x00, 0x00, 0xF0, 0x80, 0x40, 0x40, 0x80, 0x00, 0x40, 0xF0,
0x40, 0x40, 0x00, 0x00, 0x00, 0x80, 0x40, 0x40, 0x40, 0x40,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x40,
0x40, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xC0, 0x00,
0xC0, 0x80, 0x40, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9C,
0xD7, 0x03, 0x3F, 0x5C, 0x31, 0xE6, 0x88, 0x03, 0x0E, 0x7C,
0xC7, 0x1F, 0xFF, 0xFE, 0xF0, 0xC0, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
0xF0, 0xFC, 0xFF, 0x3F, 0xC7, 0xF1, 0x7C, 0x9F, 0xCF, 0x7F,
0x1F, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x04, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x07,
0x04, 0x00, 0x00, 0x13, 0x14, 0x14, 0x12, 0x0F, 0x00, 0x00,
0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x03, 0x04, 0x04,
0x02, 0x00, 0x00, 0x04, 0x05, 0x05, 0x05, 0x02, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x04, 0x04, 0x03,
0x00, 0x00, 0x03, 0x04, 0x04, 0x02, 0x07, 0x00, 0x07, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFB, 0xDB,
0xC2, 0xC6, 0x8C, 0x98, 0x31, 0x33, 0x34, 0x60, 0x61, 0xC2,
0xC0, 0x83, 0x0F, 0x1F, 0x7F, 0xFC, 0xE0, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0xC0, 0xF0, 0xFC, 0x3F, 0x1F, 0x0F, 0x03,
0x00, 0x02, 0x21, 0x20, 0x30, 0x99, 0x9C, 0x8E, 0xE7, 0xF7,
0x3F, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x80,
0x70, 0x80, 0x78, 0x00, 0x00, 0x40, 0xA8, 0xA8, 0xA8, 0xF0,
0x00, 0x38, 0xC0, 0xC0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0xDF, 0x3F,
0x7F, 0xFF, 0xFF, 0xFF, 0x3E, 0x1E, 0x0E, 0x0E, 0x07, 0x83,
0xC0, 0x00, 0x00, 0x21, 0x33, 0x33, 0x98, 0xD0, 0xC0, 0x78,
0x21, 0x63, 0xD9, 0x9C, 0x38, 0x68, 0xC8, 0xD2, 0x12, 0x22,
0x67, 0x4F, 0x79, 0x79, 0x7D, 0x3C, 0x1E, 0x06, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6F, 0x7F, 0x7E, 0x79,
0x73, 0x06, 0x0C, 0x0C, 0x0E, 0x06, 0x04, 0x06, 0x02, 0x03,
0x00, 0x06, 0x02, 0x01, 0x1D, 0x0C, 0x04, 0x00, 0x46, 0x24,
0x00, 0x01, 0x43, 0x40, 0x04, 0x11, 0x2F, 0x7E, 0x70, 0x60,
0x40, 0x70, 0x7C, 0x7C, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00
};

#endif //  ============ #ifdef runONCE_addData2EEproms  =====================

//this does complete clearing including that bottom row
void ClearDisplay() { // works in horizontal or vertical mode // NOT page mode which would only clear 1 row
for (int col=0; col<128; col++) {
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_DISPLAY_SLEEP);   //(optional) disable display output while erasing the memory
  Wire.write(ssd1306_SET_ADDRESSING); 
  Wire.write(ssd1306_ADDRESSING_VERTICAL);
  Wire.write(ssd1306_SET_COLUMN_RANGE); Wire.write(col); Wire.write(col);  // loops through single collumns
  Wire.write(ssd1306_SET_PAGE_RANGE); Wire.write(0); Wire.write(7);        // writes all Pagerows
  Wire.endTransmission();
  
  Wire.beginTransmission(ssd1306_address);  //small chunks to not exceed I2C buffer
  Wire.write(ssd1306_dataStream);
  for (int8_t row=0; row<8; row++) {        // Vertical mode from top to bottom
  Wire.write(0);
  }  //row
  Wire.endTransmission();
  }  //collumn

  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_DISPLAY_ON);
  Wire.write(ssd1306_DISPLAY_RESET);      //redundant but may prevent lockups later
  Wire.endTransmission();
}

void oledSetTextXY(int x, int row) {       //using grid of 8 rows x 21 collumns to match (5+1)=6 x (7+1)=8 fontmap
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_ADDRESSING);
  Wire.write(ssd1306_ADDRESSING_HORIZONTAL);
  Wire.write(ssd1306_SET_COLUMN_RANGE);    //set column start/end  
  Wire.write(x * ssd1306_FONT_WIDTH);      //column start = x*6 pixels per char
  Wire.write(ssd1306_PIXEL_WIDTH - 1);     //127 = column end address = 128-1
  Wire.write(ssd1306_SET_PAGE_RANGE);      //set row start/end
  Wire.write(row);                         //row start
  Wire.write(ssd1306_PAGE_HEIGHT - 1);     //7= row end, farthest corner away
  Wire.endTransmission();
}

//cascade: 1) oledWriteString -> 2) oledWriteCharacter -> pulls font from eeprom
void oledWriteString(char *characters)
{while (*characters) oledWriteCharacter(*characters++);}

void oledWriteCharacter(char character)
{ 
  uint8_t b;
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_dataStream);
  for (int i=0; i<5; i++) {
    b = EEPROM.read(((character - 0x2D)*5)+i);  //- 0x2D is first (-) character we are using here
    if(oledInvertText){b^=0xFF;}                //inverts byte for black on white text
    Wire.write(b);
  }
 if(oledInvertText){          //spacer pixel collumn between characters
    Wire.write(0xFF);         //if inverted white background
    } else {
    Wire.write(0x00);         //normal black background
    }
  Wire.endTransmission();
}

//Note if using a Caps ONLY subset of the ASCII #font, then this takes less space in the internal eeprom
//memOffset for 'tops half of numbers' = 235 // but memOffset = 535 if you load the full ascii font
//memOffset for 'bottom half of large numbers' = 378 for the Caps-ONLY font subset & =770 if you load the full ascii font

void oledWriteBigNumber(char *characters,int memOffset)
{
  while(*characters){
    WriteBigDigitHalf(*characters++,memOffset);
  }
}

void WriteBigDigitHalf(char character,int memOffset) //offset determines top or bottom half of font
{
  //byte bytebuffer;
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_dataStream);
  for(int i=0; i<11; i++){       //big digits are 11 collumns wide  (+ whatever spacers you add after)
    if((character - 0x2d)>=0){  //0x2d is offset for font starting with (-) ascii character
    Wire.write(EEPROM.read(memOffset+((character - 0x2D)*11)+i));
    //the big number font "Tops" start at eeprom memory address 235
    }
  }
  Wire.write(0x00);               // rows of empty spacer pixels
  Wire.write(0x00);               // between each character
  Wire.write(0x00);
  //Wire.write(0x00);
  Wire.endTransmission();
}


void ssd1306_HorizontalProgressBar(int8_t row,int percentComplete,int barColumnMin,int barColumnMax ) {

  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_ADDRESSING);
  Wire.write(ssd1306_ADDRESSING_HORIZONTAL);
  Wire.write(ssd1306_SET_PAGE_RANGE); Wire.write(row); Wire.write(row);
  Wire.write(ssd1306_SET_COLUMN_RANGE); Wire.write(barColumnMin); Wire.write(barColumnMax);  // one collumn ata time
  Wire.endTransmission();
  
  int changePoint = ((percentComplete*(barColumnMax-barColumnMin))/100)+barColumnMin;    
      for (int col = barColumnMin ; col < barColumnMax+1; col++) {
      Wire.beginTransmission(ssd1306_address);
      Wire.write(ssd1306_oneData);                  // 1byte at a time to avoid problem w wire.h 30 byte limit
      if (col<changePoint || col<=barColumnMin || col>=barColumnMax) {
        Wire.write(0b11111110);                     // full = all but top pixel on
        }
      else {Wire.write(0b10000010);                 // empty = edges only
        }
        Wire.endTransmission();
        } 
  }

//=============================================================================
// These graphing functions are by David Johnson-Davies
// see: Tiny Function Plotter:  http://www.technoblogy.com/show?2CFT
// I have tweaked them slightly to provide dashed background lines & a bar graph
//==============================================================================

// Gaussian approximation
int e (int x, int f, int m) {
  return (f * 256) / (256 + ((x - m) * (x - m)));
}

// Davids function uses Vertical addressing to plot the graph one collumn at a time
void PlotGraphPoint (int8_t x, int8_t y, int8_t mode, uint8_t dashSpace) {
  byte dotLinePixel=0;
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_ADDRESSING); 
  Wire.write(ssd1306_ADDRESSING_VERTICAL);
  Wire.write(ssd1306_SET_COLUMN_RANGE); Wire.write(x); Wire.write(x); // Column range - only writes in one collumn at a time
  Wire.write(ssd1306_SET_PAGE_RANGE); Wire.write(0); Wire.write(7);   // writing to ALL vertical pages
  if (!mode){
    Wire.write(ssd1306_SET_CONTRAST);
    Wire.write(0xFF);         //line only plots need the brightness boost
    }else{
    Wire.write(ssd1306_SET_CONTRAST);
    Wire.write(0x7F);}        //use default brightness for histogram
  Wire.endTransmission();
   
  if (dashSpace>0){                                // dashspacing between 3-5 looks good
    if (!(x%dashSpace)){dotLinePixel=0b00010000;}  //dotLinePixel gets OR'd in later
     else{dotLinePixel=0b00000000;}
   }

  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_dataStream);
  
  for (uint8_t h=0; h<8; h++) {                   // fill row bytes from top to bottom   
    if (y > 7) Wire.write((- mode)|dotLinePixel); // pixel not reached yet
    else if (y < 0) Wire.write(0|dotLinePixel);   // y goes negative after row containing point is reached
    else if (mode) Wire.write(((1<<y) - mode)|dotLinePixel);  // histogram does not need thickening
    else Wire.write(((1<<y) - mode)|dotLinePixel|(1<<(y-1))- mode); 
    //Note:  |(1<<(y-1))- mode)  makes the line graph two pixels thick - this can be removed for single line graph
    y = y - 8;
  }
  
  Wire.endTransmission();
}

// Simply adding repetition to PlotGraphPoint to generate bars for each single data point
void BarGraph (uint8_t x, int8_t yStart, int8_t mode) { //signed INTEGERs for the -mode trick
  int8_t y;
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream); //mode is already vertical
  Wire.write(0x21); Wire.write(x); Wire.write(x+6);  // Column range - only writ in one collumn ata time
  Wire.write(0x22); Wire.write(0); Wire.write(7);  // writing to ALL Page range
  Wire.write(ssd1306_SET_CONTRAST);
  Wire.write(0x7F); //default brightness for bar graph
  Wire.endTransmission();
  
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_dataStream);
 
  for (uint8_t q=0; q<3; q++) {         //THREE lines of pixels are drawn for each bar
  y=yStart;
      for (uint8_t i=0; i<8; i++) { 
         if (y > 7) Wire.write(- mode); // pixel not reached yet
         else if (y < 0) Wire.write(0); // y goes neg after pixel is set
         else Wire.write((1<<y) - mode);// found the page>set a pixel in the byte 
      y = y - 8;
      }
  if(x<127){x=x+1;} 
  }
  Wire.endTransmission(); //3 vertical lines is wire byte limit 
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_dataStream);  
  for (uint8_t r=0; r<3; r++) { //and trailing blank lines as separator 
  for (uint8_t s=0; s<8; s++) {Wire.write(0);}
  if(x<127){x=x+1;}
  }
  Wire.endTransmission();
}

//================bitmap splash screen functions=================================
void display_bgBitmap(void) { 

  uint16_t locationPointer=0;
  uint8_t tempByte;
  oledSetTextXY(0,0);
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);  
  Wire.write(ssd1306_SET_ADDRESSING); 
  Wire.write(ssd1306_ADDRESSING_VERTICAL); 
  Wire.endTransmission();
  //multiple passes to reach all 1024 pixels one byte at a time
  for (int collumn = 0 ; collumn < 128; collumn++) {      
      Wire.beginTransmission(ssd1306_address); 
      Wire.write(ssd1306_commandStream);
      Wire.write(ssd1306_SET_COLUMN_RANGE); Wire.write(collumn); Wire.write(collumn);  // one collumn ata time
      Wire.write(ssd1306_SET_PAGE_RANGE); Wire.write(0); Wire.write(7);  // writing all 8 pages
      Wire.endTransmission();
      
       for (int row = 0 ; row < 8; row++) {           // 0,0 point is upper left corner with com&seg scan reversed
         locationPointer = (row*128) + collumn;       // pattern matches that used when loading the data into eeprom
         Wire.beginTransmission((uint8_t)0x57);       // address of 4k eeprom on RTC module
         Wire.write((uint8_t)(locationPointer >> 8));    // MSB of the mem address
         Wire.write((uint8_t)(locationPointer & 0xFF));  // LSB of the mem address
         Wire.endTransmission(); 
         Wire.requestFrom((uint8_t)0x57,1);
         tempByte = Wire.read();
         Wire.endTransmission();
         
         Wire.beginTransmission(ssd1306_address);
         Wire.write(ssd1306_oneData);
         Wire.write(tempByte);
         Wire.endTransmission();
        }  //row 
   } //collumn    
  oledSetTextXY(0,0);
}

//==========================================================
//==========================================================
//==========================================================
void setup() {

//==========================================================
#ifdef runONCE_addData2EEproms
//==========================================================
//MOVE font data from progmem arrays into 328p EEprom memory

int currentIntEEpromAddress=0; //eeprom memory pointer

//for(int i=13; i<59; i++){       //subset of 13 to 59 caps only characters  //13=0x2d = (-)character is first
for(uint8_t i=0; i<(127-20); i++){    //complete ascii font = 107 char x 5 col = 535 bytes in eeprom
  for(uint8_t j=0; j<5; j++){      //each character of this 5x7 font has 5 collumns (+ a spacer row)
  currentIntEEpromAddress=(((i-13)*5)+j);
  charbuffer=pgm_read_byte(&ASCII[i][j]);
  EEPROM.update(currentIntEEpromAddress,charbuffer); 
  //use EEPROM.update to protect memory from accidental burnout
  }
}

//big number-only font "Top half" fills the next 143 bytes of the eeprom starting at (space needed by 1st font)
//note the proceeding 535 bytes are already taken up by the regular ASCII font
for(uint8_t i=0; i<13; i++){     // 13 characters in the array
  for(uint8_t j=0; j<11; j++){   // each font char is 11 bytes wide
  //currentIntEEpromAddress=(235+(i*11)+j);  //235 if using a Caps-ONLY subset
  currentIntEEpromAddress=(535+(i*11)+j);    //535 offset for the full ASCII font
  charbuffer=pgm_read_byte(&Big11x16numberTops[i][j]); 
  EEPROM.update(currentIntEEpromAddress,charbuffer);
  }
}

// Then big number font "Bottom half" fills the next 143 bytes of the eeprom starting @ (space for 1st font)+(space for # font 'tops')
for(uint8_t i=0; i<13; i++){
  for(uint8_t j=0; j<11; j++){
  //currentIntEEpromAddress=(378+(i*11)+j);   //if using CAPS ONLY subset
  currentIntEEpromAddress=(678+(i*11)+j);     //678 offset if using full ascii font
  charbuffer=pgm_read_byte(&Big11x16numberBottoms[i][j]); 
  EEPROM.update(currentIntEEpromAddress,charbuffer);
  }
}

// Move the bitmap 'splash screen' from progmem into the 4k at24c32 eeprom on the loggers DS3131 module
// One byte as a time, so the row/col loop pattern is obvious, but could be done more efficiently
// Note at24c32 eeprom is only rated to 100kHz for writing, but reads OK with I2C bus @ 400kHz
uint16_t addressPointer=0;
for (int collumn = 0 ; collumn < 128; collumn++) {
   for (int row = 0 ; row < 8; row++) {  //writing in vertical mode avoids wires 32byte limit  
          addressPointer = (row*128) + collumn;  //0,0 point is upper left corner with com&seg scan reversed   
          Wire.beginTransmission((uint8_t)0x57);
          Wire.write((uint8_t)((addressPointer) >> 8));   // send the MSB of the address
          Wire.write((uint8_t)((addressPointer) & 0xFF)); // send the LSB of the address
          Wire.write((uint8_t)(pgm_read_byte(&backgroundBitmap[addressPointer])));//same pointer used for both
          Wire.endTransmission();  
          delay(6);               //slow eeprom needs time to write data!
        }//row
 } //collumn    

//===========================================================================================================
#endif  // #ifdef runONCE_addData2EEproms  = these sections of code can be deleted after eeprom is poplulated
//===========================================================================================================

 Wire.begin();

 //OPTIONAL: speed up the I2C bus: ssd1306 runs fine up to >800 kHz
 // TWBR = 12; // 200 kHz I2C bus @ 8MHz CPU 
 TWBR = 2;  // 400 kHz I2C bus @ 8MHz CPU // as fast as you can go on our 3.3v loggers
 // NOTE that sensors & wire lengths may limit your your speed to the 100kHz default

 ssd1306_address = 0X3C;      //screen1 for text & progress bars: config & clear
 ssd1306_InitScreen(); 
 ClearDisplay();
 
 ssd1306_address = 0X3D;      //screen2: Graphs
 ClearDisplay();
 ssd1306_InitScreen(); 
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_CONTRAST);
  Wire.write(0);        //blanks screen by setting contrast to zero
  Wire.write(ssd1306_DISPLAY_ON);
  Wire.endTransmission();
  display_bgBitmap();
  //use the force to 'fade-in' a splash graphic
  for (int s=0; s<256; s++) {
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_CONTRAST);
  Wire.write(s);
  Wire.endTransmission();
  delay(10);
  }
}

//==========================================================
//==========================================================
//==========================================================
void loop()
{ 
  ssd1306_address = 0X3C;      //screen1: Text, Progress bars, and Graph Y-Axis label
  ClearDisplay(); //leaves screen in ON state
  
  //draw 3 character wide battery bar with inverted text below
  int batPercent = 47;
  ssd1306_HorizontalProgressBar(0,batPercent,0,17);   //(int8_t row,int percent,int graphColumnMin,int graphColumnMax )
  int BatteryVoltage=3458;      //secret voltmeter will give you 4 digit in
  float BatPointNumber = float(BatteryVoltage)/1000.0;
  oledSetTextXY(0, 1);
  oledInvertText = true;
  oledWriteString(dtostrf(BatPointNumber,3,1,tmp));
  oledInvertText = false;

  // sample interval & time text above a large progress bar:
  oledSetTextXY(5, 0);
  oledWriteString("15min 12:08P");  // populate this string w real data from your RTC
  ssd1306_HorizontalProgressBar(1,33,26,96);
  //( row, percent,  graphColumnMin, graphColumnMax )

  // axes labels along rt side of text screen for plot on the second screen:
  oledInvertText = true;
  oledSetTextXY(21, 0);
  oledWriteString("TEMP"); 
  oledSetTextXY(21, 1); 
  oledWriteString("  ;C");
  oledInvertText = false; 
  oledSetTextXY(21, 2);
  oledWriteString("|35-");
  oledSetTextXY(21, 3);
  oledWriteString("|  -");
  oledSetTextXY(21, 4);
  oledWriteString("|25-");
  oledSetTextXY(21, 5);
  oledWriteString("|  -");
  oledSetTextXY(21, 6);
  oledWriteString("|15-");
  oledSetTextXY(21, 7);
  oledWriteString("|  -");

  // data displayed in large # font variables
  // displaying large numbers on screen is a two-pass process because font is split
  float TempVariable= 27.1534;
  oledSetTextXY(0, 3);  
  oledWriteBigNumber(dtostrf(TempVariable,5,2,tmp),535);  //(string, memoffset)
  //235 is eep memory location of 'tops' for partial, 535 if you load the full ascii font
  oledSetTextXY(0, 4);  // now print the bottom half of the number font in the next row
  oledWriteBigNumber(dtostrf(TempVariable,5,2,tmp),678); //dtostrf crops to two decimal digits
  oledSetTextXY(15, 3); 
  oledWriteString(";C"); //Note: degree symbol replaces ; character in font map
  
  float PressureVariable= 1014.278;
  oledSetTextXY(0, 6);
  oledWriteBigNumber(dtostrf(PressureVariable,6,1,tmp),535); 
  oledSetTextXY(0, 7); 
  oledWriteBigNumber(dtostrf(PressureVariable,6,1,tmp),678);
  oledSetTextXY(15, 5);
  oledWriteString("mBar");
  
  delay(3000);

  ssd1306_address = 0X3D; //screen2: Graph display on right side
  //fade out splash graphic Bitmap with contrast setting
  for (int s=256; s>0; s--) {
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_SET_CONTRAST);
  Wire.write(s-1);
  Wire.endTransmission();
  delay(10);
  }
  
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_commandStream);
  Wire.write(ssd1306_DISPLAY_SLEEP);
  Wire.write(ssd1306_SET_ADDRESSING);
  Wire.write(ssd1306_ADDRESSING_VERTICAL);
  Wire.write(ssd1306_SET_COM_SCAN_DEFAULT); // for compatiblity with Tiny Function Plotter:  http://www.technoblogy.com/show?2CFT
  Wire.endTransmission();  
  ClearDisplay();
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_oneCommand);
  Wire.write(ssd1306_DISPLAY_ON);
  Wire.endTransmission();
   
  for (int x=0; x<128; x++) {       //mode = 0 = 2-pixel thick line graph
  //for (int x=128; x>0; x--) {     //changes the direction of the x axis
    int y = e(x, 40, 24) + e(x, 68, 64) + e(x, 30, 104) - 14; // e is a function to produce demo graphs, but will use data from senors later
    PlotGraphPoint(x,y,0,3);        //(collumn, row/page, mode (0=point,1=histogram), dash spacing)
  }
  
  delay(6000);
  ClearDisplay();
   
  for (int x=0; x<128; x++) {       //mode =1 = FILLED histogram style graph
    int y = e(x, 40, 24) + e(x, 68, 64) + e(x, 30, 104) - 14;
    PlotGraphPoint(x, y,1,3);       //NOTE: setting dashspace = 0 will disable dashes
  }
  
  delay(5000);
  ClearDisplay();
  
// can fit 25 bars with 3/2 combination - but you can tweak the function for other combinations
  for (int x=0; x<128; x+=5) { //use x+=5 because 3-line wide bars + separated by 2 blank lines
    int y = e(x, 40, 24) + e(x, 68, 64) + e(x, 30, 104) - 14;
    BarGraph(x,y,1);                 // mode not used - bars are solid fill
    delay(200);                      // delay here to simulate lag from 'live' sensor reads // remove later
  }
  delay(5000);

  //to turn off the displays:
  ssd1306_address = 0X3C;     //screen1: text & progress bars
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_oneCommand);
  Wire.write(ssd1306_DISPLAY_SLEEP);
  Wire.endTransmission();
  
  ssd1306_address = 0X3D;     //screen2: graph data
  Wire.beginTransmission(ssd1306_address);
  Wire.write(ssd1306_oneCommand);
  Wire.write(ssd1306_DISPLAY_SLEEP);
  Wire.endTransmission();
  
  delay(2000);
}// end of main loop
