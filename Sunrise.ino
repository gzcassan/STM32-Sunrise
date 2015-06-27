/*.
(c) Andrew Hull - 2015

STM32-O-Scope - aka "The Pig Scope" or pigScope released under the GNU GENERAL PUBLIC LICENSE Version 2, June 1991

https://github.com/pingumacpenguin/STM32-O-Scope

Adafruit Libraries released under their specific licenses Copyright (c) 2013 Adafruit Industries.  All rights reserved.

*/

#include "Adafruit_ILI9341_STM.h"
#include "Adafruit_GFX_AS.h"

// Be sure to use the latest version of the SPI libraries see stm32duino.com - http://stm32duino.com/viewtopic.php?f=13&t=127
#include <SPI.h>

#define PORTRAIT 0
#define LANDSCAPE 1

// Define the orientation of the touch screen. Further
// information can be found in the UTouch library documentation.

#define TOUCH_SCREEN_AVAILABLE
#define TOUCH_ORIENTATION  LANDSCAPE

#if defined TOUCH_SCREEN_AVAILABLE
// UTouch Library
// http://www.rinkydinkelectronics.com/library.php?id=56
#include <UTouch.h>

#endif
// Initialize touchscreen
// ----------------------
// Set the pins to the correct ones for your STM32F103 board
// -----------------------------------------------------------

//
// STM32F103C8XX Pin numbers - chosen for ease of use on the "Red Pill" and "Blue Pill" board

// Touch Panel Pins
// T_CLK T_CS T_DIN T_DOUT T_IRQ
// PB12 PB13 PB14 PB15 PA8
// Example wire colours Brown,Red,Orange,Yellow,Violet
// --------             Brown,Red,Orange,White,Grey

#if defined TOUCH_SCREEN_AVAILABLE

UTouch  myTouch( PB12, PB13, PB14, PB15, PA8);

#endif

// RTC and NVRam initialisation

#include "RTClock.h"
RTClock rt (RTCSEL_LSE); // initialise
uint32 tt;

// Define the Base address of the RTC  registers (battery backed up CMOS Ram), so we can use them for config of touch screen and other calibration.
// See http://stm32duino.com/viewtopic.php?f=15&t=132&hilit=rtc&start=40 for a more details about the RTC NVRam
// 10x 16 bit registers are available on the STM32F103CXXX more on the higher density device.

#define BKP_REG_BASE   (uint32_t *)(0x40006C00 +0x04)

// Defined for power and sleep functions pwr.h and scb.h
#include <libmaple/pwr.h>
#include <libmaple/scb.h>


// #define NVRam register names for the touch calibration values.
#define  TOUCH_CALIB_X 0
#define  TOUCH_CALIB_Y 1
#define  TOUCH_CALIB_Z 2

// Time library - https://github.com/PaulStoffregen/Time
#include "Time.h"
#define TZ    "UTC+1"

// End RTC and NVRam initialization

// SeralCommand -> https://github.com/kroimon/Arduino-SerialCommand.git
#include <SerialCommand.h>

/* For reference on STM32F103CXXX

variants/generic_stm32f103c/board/board.h:#define BOARD_NR_SPI              2
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI1_NSS_PIN        PA4
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI1_MOSI_PIN       PA7
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI1_MISO_PIN       PA6
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI1_SCK_PIN        PA5

variants/generic_stm32f103c/board/board.h:#define BOARD_SPI2_NSS_PIN        PB12
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI2_MOSI_PIN       PB15
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI2_MISO_PIN       PB14
variants/generic_stm32f103c/board/board.h:#define BOARD_SPI2_SCK_PIN        PB13

*/

// Additional  display specific signals (i.e. non SPI) for STM32F103C8T6 (Wire colour)
#define TFT_DC        PA0      //   (Green) 
#define TFT_CS        PA1      //   (Orange) 
#define TFT_RST       PA2      //   (Yellow)

// Hardware SPI1 on the STM32F103C8T6 *ALSO* needs to be connected and pins are as follows.
//
// SPI1_NSS  (PA4) (LQFP48 pin 14)    (n.c.)
// SPI1_SCK  (PA5) (LQFP48 pin 15)    (Brown)
// SPI1_MOSO (PA6) (LQFP48 pin 16)    (White)
// SPI1_MOSI (PA7) (LQFP48 pin 17)    (Grey)
//

#define TFT_LED        PA3     // Backlight 
#define TEST_WAVE_PIN       PB0     // PWM 500 Hz 

// Create the lcd object
Adafruit_ILI9341_STM TFT = Adafruit_ILI9341_STM(TFT_CS, TFT_DC, TFT_RST); // Using hardware SPI

// LED - blinks on trigger events - leave this undefined if your board has no controllable LED
// define as PC13 on the "Red/Blue Pill" boards and PD2 on the "Yellow Pill R"
#define BOARD_LED PD2

// Display colours
#define BEAM1_COLOUR ILI9341_GREEN
#define BEAM2_COLOUR ILI9341_RED
#define GRATICULE_COLOUR 0x07FF
#define BEAM_OFF_COLOUR ILI9341_BLACK
#define CURSOR_COLOUR ILI9341_GREEN

// Screen dimensions
int16_t myWidth ;
int16_t myHeight ;

// Create Serial Command Object.
SerialCommand sCmd;

// Create USB serial port
USBSerial serial_debug;


// Sunrise stuff
#include <math.h>
#define PI 3.1415926
#define ZENITH -.83
/*
#define Stirling_Latitude 56.138607900000000000
#define Stirling_Longitude -3.944080100000064700
*/

// Lat long for your location
#define HERE_LATITUDE  56.1386079
#define HERE_LONGITUDE -3.9440801

int thisYear = 2015;
int thisMonth = 6;
int thisDay = 27;

float thisLat = HERE_LATITUDE;
float thisLong = HERE_LONGITUDE;
int thisLocalOffset = 0;
int thisDaylightSavings = 1;


void setup()
{

  // BOARD_LED blinks on events assuming you have an LED on your board. If not simply dont't define it at the start of the sketch.
#if defined BOARD_LED
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, HIGH);
  delay(1000);
  digitalWrite(BOARD_LED, LOW);
  delay(1000);
#endif

  serial_debug.begin();

  //
  // Serial command setup
  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("timestamp",   setCurrentTime);          // Set the current time based on a unix timestamp
  sCmd.addCommand("date",        serialCurrentTime);       // Show the current time from the RTC
  sCmd.addCommand("sleep",       sleepMode);               // Experimental - puts system to sleep

#if defined TOUCH_SCREEN_AVAILABLE
//  sCmd.addCommand("touchcalibrate", touchCalibrate);       // Calibrate Touch Panel
#endif


  sCmd.setDefaultHandler(unrecognized);                    // Handler for command that isn't matched  (says "Unknown")
  sCmd.clearBuffer();

  // Backlight, use with caution, depending on your display, you may exceed the max current per pin if you use this method.
  // A safer option would be to add a suitable transistor capable of sinking or sourcing 100mA (the ILI9341 backlight on my display is quoted as drawing 80mA at full brightness)
  // Alternatively, connect the backlight to 3v3 for an always on, bright display.
  //pinMode(TFT_LED, OUTPUT);
  //analogWrite(TFT_LED, 127);


  // Setup Touch Screen
  // http://www.rinkydinkelectronics.com/library.php?id=56
#if defined TOUCH_SCREEN_AVAILABLE
  myTouch.InitTouch();
  myTouch.setPrecision(PREC_EXTREME);
#endif



  TFT.begin();
  // initialize the display
  clearTFT();
  TFT.setRotation(PORTRAIT);
  myHeight   = TFT.width() ;
  myWidth  = TFT.height();
  TFT.setTextColor(CURSOR_COLOUR, BEAM_OFF_COLOUR) ;
#if defined TOUCH_SCREEN_AVAILABLE
//  touchCalibrate();
#endif

  TFT.setRotation(LANDSCAPE);
  clearTFT();
  //showCredits(); // Honourable mentions ;¬)
  //showGraticule();
  delay(5000) ;
  clearTFT();
//  notTriggered = true;
//  showGraticule();
//  showLabels();
}

void loop() {
  // put your main code here, to run repeatedly:
  showTime();
  printSunrise();
  serialCurrentTime();

}



float calculateSunrise(int year, int month, int day, float lat, float lng, int localOffset, int daylightSavings) {
  /*
  localOffset will be <0 for western hemisphere and >0 for eastern hemisphere
  daylightSavings should be 1 if it is in effect during the summer otherwise it should be 0
  */
  //1. first calculate the day of the year
  float N1 = floor(275 * month / 9);
  float N2 = floor((month + 9) / 12);
  float N3 = (1 + floor((year - 4 * floor(year / 4) + 2) / 3));
  float N = N1 - (N2 * N3) + day - 30;

  //2. convert the longitude to hour value and calculate an approximate time
  float lngHour = lng / 15.0;
  float t = N + ((6 - lngHour) / 24);   //if rising time is desired:
  //float t = N + ((18 - lngHour) / 24)   //if setting time is desired:

  //3. calculate the Sun's mean anomaly
  float M = (0.9856 * t) - 3.289;

  //4. calculate the Sun's true longitude
  float L = fmod(M + (1.916 * sin((PI / 180) * M)) + (0.020 * sin(2 * (PI / 180) * M)) + 282.634, 360.0);

  //5a. calculate the Sun's right ascension
  float RA = fmod(180 / PI * atan(0.91764 * tan((PI / 180) * L)), 360.0);

  //5b. right ascension value needs to be in the same quadrant as L
  float Lquadrant  = floor( L / 90) * 90;
  float RAquadrant = floor(RA / 90) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  //5c. right ascension value needs to be converted into hours
  RA = RA / 15;

  //6. calculate the Sun's declination
  float sinDec = 0.39782 * sin((PI / 180) * L);
  float cosDec = cos(asin(sinDec));

  //7a. calculate the Sun's local hour angle
  float cosH = (sin((PI / 180) * ZENITH) - (sinDec * sin((PI / 180) * lat))) / (cosDec * cos((PI / 180) * lat));
  /*
  if (cosH >  1)
  the sun never rises on this location (on the specified date)
  if (cosH < -1)
  the sun never sets on this location (on the specified date)
  */

  //7b. finish calculating H and convert into hours
  float H = 360 - (180 / PI) * acos(cosH); //   if if rising time is desired:
  //float H = acos(cosH) //   if setting time is desired:
  H = H / 15;

  //8. calculate local mean time of rising/setting
  float T = H + RA - (0.06571 * t) - 6.622;

  //9. adjust back to UTC
  float UT = fmod(T - lngHour, 24.0);

  //10. convert UT value to local time zone of latitude/longitude
  return UT + localOffset + daylightSavings;

}

void printSunrise() {
    TFT.setTextSize(4);
   TFT.setCursor(10, 120);
  float localT = calculateSunrise(thisYear, thisMonth, thisDay, thisLat, thisLong, thisLocalOffset, thisDaylightSavings);
  double hours;
  float minutes = modf(localT, &hours) * 60;
  TFT.print("Sun Up ");
  TFT.print(uint(hours));
  TFT.print(":");
  TFT.print(uint(minutes));
  // TFT.printf("%.0f:%.0f", hours, minutes);
}

void setCurrentTime() {
  char *arg;
  arg = sCmd.next();
  String thisArg = arg;
  serial_debug.print("# Time command [");
  serial_debug.print(thisArg.toInt() );
  serial_debug.println("]");
  setTime(thisArg.toInt());
  time_t tt = now();
  rt.setTime(tt);
  serialCurrentTime();
}

void serialCurrentTime() {
  serial_debug.print("# Current time - ");
  if (hour(tt) < 10) {
    serial_debug.print("0");
  }
  serial_debug.print(hour(tt));
  serial_debug.print(":");
  if (minute(tt) < 10) {
    serial_debug.print("0");
  }
  serial_debug.print(minute(tt));
  serial_debug.print(":");
  if (second(tt) < 10) {
    serial_debug.print("0");
  }
  serial_debug.print(second(tt));
  serial_debug.print(" ");
  serial_debug.print(day(tt));
  serial_debug.print("/");
  serial_debug.print(month(tt));
  serial_debug.print("/");
  serial_debug.print(year(tt));
  serial_debug.println("("TZ")");
}

void unrecognized(const char *command) {
  serial_debug.print("# Unknown Command.[");
  serial_debug.print(command);
  serial_debug.println("]");
}

void clearTFT()
{
  TFT.fillScreen(BEAM_OFF_COLOUR);                // Blank the display
}

void showTime ()
{
  // Show RTC Time.
  TFT.setTextSize(4);
  //TFT.setRotation(LANDSCAPE);
  if (rt.getTime() != tt)
  {
    tt = rt.getTime();
    TFT.setCursor(5, 10);
    if (hour(tt) < 10) {
      TFT.print("0");
    }
    TFT.print(hour(tt));
    TFT.print(":");
    if (minute(tt) < 10) {
      TFT.print("0");
    }
    TFT.print(minute(tt));
    TFT.print(":");
    if (second(tt) < 10) {
      TFT.print("0");
    }
    TFT.print(second(tt));
    TFT.print(" ");
    TFT.print(day(tt));
    TFT.print("-");
    TFT.print(month(tt));
    TFT.print("-");
    TFT.print(year(tt));
    TFT.print(" "TZ" ");
    // TFT.print(tt);

  }
  //TFT.setRotation(PORTRAIT);
}
void sleepMode() 
{
  serial_debug.println("# Nighty night!");
  // Set PDDS and LPDS bits for standby mode, and set Clear WUF flag (required per datasheet):
PWR_BASE->CR |= PWR_CR_CWUF;
PWR_BASE->CR |= PWR_CR_PDDS;

// set sleepdeep in the system control register
SCB_BASE->SCR |= SCB_SCR_SLEEPDEEP;

// Now go into stop mode, wake up on interrupt
// disableClocks();
asm("wfi");
}
