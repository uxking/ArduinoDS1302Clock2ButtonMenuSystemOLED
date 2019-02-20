// ***** DS1302 RTC with SoftTimer, OLED, and Time/Date Setting Menu     ******
// ***** Initial DS1302 downloaded from Arduino Forums by user "Krodal". ****** 
// ***** Thx Krodal                                                      ******
// ----------
//
// Open Source / Public Domain
//
// Version 1
//     By arduino.cc user "Krodal".
//     June 2012
//     Using Arduino 1.0.1
// Version 2
//     By arduino.cc user "Krodal"
//     March 2013
//     Using Arduino 1.0.3, 1.5.2
//     The code is no longer compatible with older versions.
//     Added bcd2bin, bin2bcd_h, bin2bcd_l
//     A few minor changes.
//
//
// Documentation: datasheet
// 
// The DS1302 uses a 3-wire interface: 
//    - bidirectional data.
//    - clock
//    - chip select
// It is not I2C, not OneWire, and not SPI.
// So the standard libraries can not be used.
// Even the shiftOut() function is not used, since it
// could be too fast (it might be slow enough, 
// but that's not certain).
//
// I wrote my own interface code according to the datasheet.
// Any three pins of the Arduino can be used.
//   See the first defines below this comment, 
//   to set your own pins.
//
// The "Chip Enable" pin was called "/Reset" before.
//
// The chip has internal pull-down registers.
// This keeps the chip disabled, even if the pins of 
// the Arduino are floating.
//
//
// Range
// -----
//      seconds : 00-59
//      minutes : 00-59
//      hour    : 1-12 or 0-23
//      date    : 1-31
//      month   : 1-12
//      day     : 1-7
//      year    : 00-99
//
//
// Burst mode
// ----------
// In burst mode, all the clock data is read at once.
// This is to prevent a rollover of a digit during reading.
// The read data is from an internal buffer.
//
// The burst registers are commands, rather than addresses.
// Clock Data Read in Burst Mode
//    Start by writing 0xBF (as the address), 
//    after that: read clock data
// Clock Data Write in Burst Mode
//    Start by writing 0xBE (as the address), 
//    after that: write clock data
// Ram Data Read in Burst Mode
//    Start by writing 0xFF (as the address), 
//    after that: read ram data
// Ram Data Write in Burst Mode
//    Start by writing 0xFE (as the address), 
//    after that: write ram data
//
//
// Ram
// ---
// The DS1302 has 31 of ram, which can be used to store data.
// The contents will be lost if the Arduino is off, 
// and the backup battery gets empty.
// It is better to store data in the EEPROM of the Arduino.
// The burst read or burst write for ram is not implemented 
// in this code.
//
//
// Trickle charge
// --------------
// The DS1302 has a build-in trickle charger.
// That can be used for example with a lithium battery 
// or a supercap.
// Using the trickle charger has not been implemented 
// in this code.
//
// --------------
#include <SoftTimer.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
// -- Pin change interrupt
#include <PciManager.h>
#include <Debouncer.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Set your own pins with these defines !
#define DS1302_SCLK_PIN   11    // Arduino pin for the Serial Clock/ATMEGA 17
#define DS1302_IO_PIN     10    // Arduino pin for the Data I/O/ATMEGA 16
#define DS1302_CE_PIN     9    // Arduino pin for the Chip Enable/ATMEGA 15


// Macros to convert the bcd values of the registers to normal
// integer variables.
// The code uses separate variables for the high byte and the low byte
// of the bcd, so these macros handle both bytes separately.
#define bcd2bin(h,l)    (((h)*10) + (l))
#define bin2bcd_h(x)   ((x)/10)
#define bin2bcd_l(x)    ((x)%10)


// Register names.
// Since the highest bit is always '1', 
// the registers start at 0x80
// If the register is read, the lowest bit should be '1'.
#define DS1302_SECONDS           0x80
#define DS1302_MINUTES           0x82
#define DS1302_HOURS             0x84
#define DS1302_DATE              0x86
#define DS1302_MONTH             0x88
#define DS1302_DAY               0x8A
#define DS1302_YEAR              0x8C
#define DS1302_ENABLE            0x8E
#define DS1302_TRICKLE           0x90
#define DS1302_CLOCK_BURST       0xBE
#define DS1302_CLOCK_BURST_WRITE 0xBE
#define DS1302_CLOCK_BURST_READ  0xBF
#define DS1302_RAMSTART          0xC0
#define DS1302_RAMEND            0xFC
#define DS1302_RAM_BURST         0xFE
#define DS1302_RAM_BURST_WRITE   0xFE
#define DS1302_RAM_BURST_READ    0xFF



// Defines for the bits, to be able to change 
// between bit number and binary definition.
// By using the bit number, using the DS1302 
// is like programming an AVR microcontroller.
// But instead of using "(1<<X)", or "_BV(X)", 
// the Arduino "bit(X)" is used.
#define DS1302_D0 0
#define DS1302_D1 1
#define DS1302_D2 2
#define DS1302_D3 3
#define DS1302_D4 4
#define DS1302_D5 5
#define DS1302_D6 6
#define DS1302_D7 7


// Bit for reading (bit in address)
#define DS1302_READBIT DS1302_D0 // READBIT=1: read instruction

// Bit for clock (0) or ram (1) area, 
// called R/C-bit (bit in address)
#define DS1302_RC DS1302_D6

// Seconds Register
#define DS1302_CH DS1302_D7   // 1 = Clock Halt, 0 = start

// Hour Register
#define DS1302_AM_PM DS1302_D5 // 0 = AM, 1 = PM
#define DS1302_12_24 DS1302_D7 // 0 = 24 hour, 1 = 12 hour

// Enable Register
#define DS1302_WP DS1302_D7   // 1 = Write Protect, 0 = enabled

// Trickle Register
#define DS1302_ROUT0 DS1302_D0
#define DS1302_ROUT1 DS1302_D1
#define DS1302_DS0   DS1302_D2
#define DS1302_DS1   DS1302_D2
#define DS1302_TCS0  DS1302_D4
#define DS1302_TCS1  DS1302_D5
#define DS1302_TCS2  DS1302_D6
#define DS1302_TCS3  DS1302_D7


// Structure for the first 8 registers.
// These 8 bytes can be read at once with 
// the 'clock burst' command.
// Note that this structure contains an anonymous union.
// It might cause a problem on other compilers.
typedef struct ds1302_struct
{
  uint8_t Seconds:4;      // low decimal digit 0-9
  uint8_t Seconds10:3;    // high decimal digit 0-5
  uint8_t CH:1;           // CH = Clock Halt
  uint8_t Minutes:4;
  uint8_t Minutes10:3;
  uint8_t reserved1:1;
  union
  {
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:2;
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 0 for 24 hour format
    } h24;
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:1;
      uint8_t AM_PM:1;      // 0 for AM, 1 for PM
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 1 for 12 hour format
    } h12;
  };
  uint8_t Date:4;           // Day of month, 1 = first day
  uint8_t Date10:2;
  uint8_t reserved3:2;
  uint8_t Month:4;          // Month, 1 = January
  uint8_t Month10:1;
  uint8_t reserved4:3;
  uint8_t Day:3;            // Day of week, 1 = first day (any day)
  uint8_t reserved5:5;
  uint8_t Year:4;           // Year, 0 = year 2000
  uint8_t Year10:4;
  uint8_t reserved6:7;
  uint8_t WP:1;             // WP = Write Protect
};

// mHayslip code - 2/18/2019 begins here
ds1302_struct rtc; // define once so we don't have to create this struct every time we set the time

// PCI manager listeners, pins defined for button presses (input pin is to start
// showing the set menu, value pin will change values of the time/date values
#define INPUT_PIN 8
#define VALUE_PIN 7
void onPressed(); // this is the set button
void onReleased(unsigned long pressTimeSpan);
void changeValue(); // this is the value change button

// setup the buttons for debounce
Debouncer debouncer(INPUT_PIN, MODE_CLOSE_ON_PUSH, onPressed, onReleased);
Debouncer debouncer1(VALUE_PIN, MODE_CLOSE_ON_PUSH, changeValue, onReleased);

// SoftTimer method signatures
void showTheTimeCallback(Task* me);
void showSetMenuCallback(Task* me);
Task t1_showTheTime(1000, showTheTimeCallback); // run this every second
Task t2_showSetMenu(0, showSetMenuCallback); // this runs constantly when it is showing

bool inMenu = false; // initially, we are not in the set menu
int c = 0; // number of btn presses for set button
int oldval; // used to save the values of the time to display in the set menu

char* ampm = "AM"; // Set intial value of AM
char* type; // keep a record of what we are setting, hour, minute, month, day, year

void setup()
{      
  Serial.begin(9600);
  // run debouncers
  PciManager.registerListener(INPUT_PIN, &debouncer);
  PciManager.registerListener(VALUE_PIN, &debouncer1);
  Serial.println(F("DS1302 Real Time Clock"));
  Serial.println(F("Version 2, March 2013"));
   // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  // start showing the time
  SoftTimer.add(&t1_showTheTime);
}
// mH code - ends

//void loop()
//{
//  Not needed since all the work is done via SoftTimer Tasks
//
//}


// More original DS1320 code from Arduino Forums
// --------------------------------------------------------
// DS1302_clock_burst_read
//
// This function reads 8 bytes clock data in burst mode
// from the DS1302.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_clock_burst_read( uint8_t *p)
{
  int i;

  _DS1302_start();

  // Instead of the address, 
  // the CLOCK_BURST_READ command is issued
  // the I/O-line is released for the data
  _DS1302_togglewrite( DS1302_CLOCK_BURST_READ, true);  

  for( i=0; i<8; i++)
  {
    *p++ = _DS1302_toggleread();
  }
  _DS1302_stop();
}


// --------------------------------------------------------
// DS1302_clock_burst_write
//
// This function writes 8 bytes clock data in burst mode
// to the DS1302.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_clock_burst_write( uint8_t *p)
{
  int i;

  _DS1302_start();

  // Instead of the address, 
  // the CLOCK_BURST_WRITE command is issued.
  // the I/O-line is not released
  _DS1302_togglewrite( DS1302_CLOCK_BURST_WRITE, false);  

  for( i=0; i<8; i++)
  {
    // the I/O-line is not released
    _DS1302_togglewrite( *p++, false);  
  }
  _DS1302_stop();
}


// --------------------------------------------------------
// DS1302_read
//
// This function reads a byte from the DS1302 
// (clock or ram).
//
// The address could be like "0x80" or "0x81", 
// the lowest bit is set anyway.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
uint8_t DS1302_read(int address)
{
  uint8_t data;

  // set lowest bit (read bit) in address
  bitSet( address, DS1302_READBIT);  

  _DS1302_start();
  // the I/O-line is released for the data
  _DS1302_togglewrite( address, true);  
  data = _DS1302_toggleread();
  _DS1302_stop();

  return (data);
}


// --------------------------------------------------------
// DS1302_write
//
// This function writes a byte to the DS1302 (clock or ram).
//
// The address could be like "0x80" or "0x81", 
// the lowest bit is cleared anyway.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_write( int address, uint8_t data)
{
  // clear lowest bit (read bit) in address
  bitClear( address, DS1302_READBIT);   

  _DS1302_start();
  // don't release the I/O-line
  _DS1302_togglewrite( address, false); 
  // don't release the I/O-line
  _DS1302_togglewrite( data, false); 
  _DS1302_stop();  
}


// --------------------------------------------------------
// _DS1302_start
//
// A helper function to setup the start condition.
//
// An 'init' function is not used.
// But now the pinMode is set every time.
// That's not a big deal, and it's valid.
// At startup, the pins of the Arduino are high impedance.
// Since the DS1302 has pull-down resistors, 
// the signals are low (inactive) until the DS1302 is used.
void _DS1302_start( void)
{
  digitalWrite( DS1302_CE_PIN, LOW); // default, not enabled
  pinMode( DS1302_CE_PIN, OUTPUT);  

  digitalWrite( DS1302_SCLK_PIN, LOW); // default, clock low
  pinMode( DS1302_SCLK_PIN, OUTPUT);

  pinMode( DS1302_IO_PIN, OUTPUT);

  digitalWrite( DS1302_CE_PIN, HIGH); // start the session
  delayMicroseconds( 4);           // tCC = 4us
}


// --------------------------------------------------------
// _DS1302_stop
//
// A helper function to finish the communication.
//
void _DS1302_stop(void)
{
  // Set CE low
  digitalWrite( DS1302_CE_PIN, LOW);

  delayMicroseconds( 4);           // tCWH = 4us
}


// --------------------------------------------------------
// _DS1302_toggleread
//
// A helper function for reading a byte with bit toggle
//
// This function assumes that the SCLK is still high.
//
uint8_t _DS1302_toggleread( void)
{
  uint8_t i, data;

  data = 0;
  for( i = 0; i <= 7; i++)
  {
    // Issue a clock pulse for the next databit.
    // If the 'togglewrite' function was used before 
    // this function, the SCLK is already high.
    digitalWrite( DS1302_SCLK_PIN, HIGH);
    delayMicroseconds( 1);

    // Clock down, data is ready after some time.
    digitalWrite( DS1302_SCLK_PIN, LOW);
    delayMicroseconds( 1);        // tCL=1000ns, tCDD=800ns

    // read bit, and set it in place in 'data' variable
    bitWrite( data, i, digitalRead( DS1302_IO_PIN)); 
  }
  return( data);
}


// --------------------------------------------------------
// _DS1302_togglewrite
//
// A helper function for writing a byte with bit toggle
//
// The 'release' parameter is for a read after this write.
// It will release the I/O-line and will keep the SCLK high.
//
void _DS1302_togglewrite( uint8_t data, uint8_t release)
{
  int i;

  for( i = 0; i <= 7; i++)
  { 
    // set a bit of the data on the I/O-line
    digitalWrite( DS1302_IO_PIN, bitRead(data, i));  
    delayMicroseconds( 1);     // tDC = 200ns

    // clock up, data is read by DS1302
    digitalWrite( DS1302_SCLK_PIN, HIGH);     
    delayMicroseconds( 1);     // tCH = 1000ns, tCDH = 800ns

    if( release && i == 7)
    {
      // If this write is followed by a read, 
      // the I/O-line should be released after 
      // the last bit, before the clock line is made low.
      // This is according the datasheet.
      // I have seen other programs that don't release 
      // the I/O-line at this moment,
      // and that could cause a shortcut spike 
      // on the I/O-line.
      pinMode( DS1302_IO_PIN, INPUT);

      // For Arduino 1.0.3, removing the pull-up is no longer needed.
      // Setting the pin as 'INPUT' will already remove the pull-up.
      // digitalWrite (DS1302_IO, LOW); // remove any pull-up  
    }
    else
    {
      digitalWrite( DS1302_SCLK_PIN, LOW);
      delayMicroseconds( 1);       // tCL=1000ns, tCDD=800ns
    }
  }
}

// mH - Revamped original code here to set the time via a function
// Fill these variables with the date and time.
int minutes, hours, dayofmonth, month, year;
void setTheTime(int hours, int minutes, int month, int dayofmonth, int year)
{      

  Serial.println(F("Gonna write time"));
  char buffer[20];
  sprintf(buffer, "%02d:%02d %02d/%02d/%d", hours, minutes, month, dayofmonth, year);
  Serial.println(buffer);
  // Start by clearing the Write Protect bit
  // Otherwise the clock data cannot be written
  // The whole register is written, 
  // but the WP-bit is the only bit in that register.
  DS1302_write (DS1302_ENABLE, 0);

  // Disable Trickle Charger.
  DS1302_write (DS1302_TRICKLE, 0x00);

// Remove the next define, 
// after the right date and time are set.
//#define SET_DATE_TIME_JUST_ONCE
//#ifdef SET_DATE_TIME_JUST_ONCE  


  // Example for april 15, 2013, 10:08, monday is 2nd day of Week.
  // Set your own time and date in these variables.
  int seconds    = 0;
  //minutes    = rminutes;
  //hours      = rhours;
  int dayofweek  = 1;  // Day of week, any day can be first, counts 1...7
  //dayofmonth = rdayofmonth; // Day of month, 1...31
  //month      = rmonth;  // month 1...12
  //year       = ryear;

  // Set a time and date
  // This also clears the CH (Clock Halt) bit, 
  // to start the clock.

  // Fill the structure with zeros to make 
  // any unused bits zero
  memset ((char *) &rtc, 0, sizeof(rtc));

  rtc.Seconds    = bin2bcd_l( seconds);
  rtc.Seconds10  = bin2bcd_h( seconds);
  rtc.CH         = 0;      // 1 for Clock Halt, 0 to run;
  rtc.Minutes    = bin2bcd_l( minutes);
  rtc.Minutes10  = bin2bcd_h( minutes);
  // To use the 12 hour format,
  // use it like these four lines:
  //    rtc.h12.Hour   = bin2bcd_l( hours);
  //    rtc.h12.Hour10 = bin2bcd_h( hours);
  //    rtc.h12.AM_PM  = 0;     // AM = 0
  //    rtc.h12.hour_12_24 = 0; // 1 for 24 hour format
  rtc.h24.Hour   = bin2bcd_l( hours);
  rtc.h24.Hour10 = bin2bcd_h( hours);
  rtc.h24.hour_12_24 = 0; // 0 for 24 hour format
  rtc.Date       = bin2bcd_l( dayofmonth);
  rtc.Date10     = bin2bcd_h( dayofmonth);
  rtc.Month      = bin2bcd_l( month);
  rtc.Month10    = bin2bcd_h( month);
  rtc.Day        = dayofweek;
  rtc.Year       = bin2bcd_l( year - 2000);
  rtc.Year10     = bin2bcd_h( year - 2000);
  rtc.WP = 0;  

  // Write all clock data at once (burst mode).
  DS1302_clock_burst_write( (uint8_t *) &rtc);
//#endif
}

// task will run to show the time every second
void showTheTimeCallback(Task* me) 
{
  char buffer[20];     // the code uses 6 characters.
  
  // Read all clock data at once (burst mode).
  DS1302_clock_burst_read( (uint8_t *) &rtc);

  // get current hr to use for AM/PM value - couldn't get original code to show me 12 hr time properly
  int cHr = bcd2bin(rtc.h24.Hour10, rtc.h24.Hour);
  // Set AM/PM variable
  if (cHr == 12) {
    ampm = "PM";
  }
  if (cHr > 12) {
    cHr -=12;
    ampm = "PM";
  }
  if (cHr == 0) {
    cHr = 12;
    ampm = "AM";
  }
  // get current seconds to display in the corner of the OLED
  int secs = bcd2bin( rtc.Seconds10, rtc.Seconds);
  sprintf(buffer, "%02d:%02d", cHr, bcd2bin( rtc.Minutes10, rtc.Minutes));
  //Serial.println(buffer);
  // show the time on the OLED
  display.clearDisplay();
  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 32);
  display.print(buffer);
  display.setTextSize(1);
  display.setCursor(100, 12);
  sprintf(buffer, "%02d", secs);
  display.print(buffer);
  display.setCursor(100, 32);
  display.print(ampm);

  sprintf( buffer, "%02d/%02d/%02d", \
    bcd2bin( rtc.Month10, rtc.Month), \
    bcd2bin( rtc.Date10, rtc.Date), \
    2000 + bcd2bin (rtc.Year10, rtc.Year));
  //Serial.println(buffer);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20, 60);
  display.print(buffer);
  display.display();

}
// need vars to save old values when we push the set button
int oldhr, oldmin, oldmonth, oldday, oldyear;
// Set button pressed
void onPressed() {
  // keep count of how many times it was pressed
  c++;
  if (c == 1) {
    // need to know when we are in menu mode
    inMenu = true;
    // stop displaying the time
    SoftTimer.remove(&t1_showTheTime);
    // get current values to display on the set menu
    oldhr = bcd2bin( rtc.h24.Hour10, rtc.h24.Hour);
    oldmin = bcd2bin( rtc.Minutes10, rtc.Minutes); 
    oldmonth = bcd2bin (rtc.Month10, rtc.Month);
    oldday = bcd2bin (rtc.Date10, rtc.Date);
    oldyear = bcd2bin (rtc.Year10, rtc.Year);
    // start displaying the set menu
    SoftTimer.add(&t2_showSetMenu);
  } else if (c == 6) { // final number of button presses means we are done with the setting
    // stop displaying the set menu
    SoftTimer.remove(&t2_showSetMenu);
    // set the time from what we got from the set menu
    setTheTime(hours, minutes, month, dayofmonth, year);
    // start displaying the time again
    SoftTimer.add(&t1_showTheTime);
    // reset count of number of set button pushes to zero
    c = 0;
    // set bool to let us know we are no longer in menu mode
    inMenu = false;
  }
}
// SoftTimer task to show start showing the menu
void showSetMenuCallback(Task* me) {
  // start the display - send the number of button clicks to the function
  showMenu(c);
}
// function shows the current menu item based on count of button presses
void showMenu(int c) {
  char buffer[20]; // buffer to hold characters for printing
  // find the number of set button presses and display appropriate values, if we 
  // never change values, then use the old values to set the time
  if (c == 1) {
    type = "Hour";
    oldval = oldhr;
    hours = oldhr;
  } else
  if (c == 2) {
    type = "Min";
    oldval = oldmin;
    minutes = oldmin;
  } else
  if (c == 3) {
    type = "Month";
    oldval = oldmonth;
    month = oldmonth;
  } else
  if (c == 4) {
    type = "Day";
    oldval = oldday;
    dayofmonth = oldday;
  } else
  if (c == 5) {
    type = "Year";
    oldval = oldyear;
    year = oldyear + 2000;
  }
  // OLED will display the current value as oldval when we are in the set menu
  sprintf(buffer, "Set %s", type);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 24);
  display.print(buffer);
  display.setTextSize(1);
  display.setCursor(32, 46);
  display.print(oldval);
  display.display();
}
//Function needed for debounce
void onReleased(unsigned long pressTimespan) {
  //Serial.print(pressTimespan);
  //Serial.println(F(" - released"));
}

// function runs when we press the changevalue button
void changeValue() {
  if (inMenu) { // pushing the change value button on VALUE_PIN does nothing until we are in menu mode
    if (type == "Hour") {
      oldhr++;
      if (oldhr > 23) {
        oldhr = 0;
      }
      oldval = oldhr; 
      hours = oldhr;
    } else if (type == "Min") {
       oldmin++;
       if (oldmin > 59) {
         oldmin = 0;   
       }
       oldval = oldmin;
       minutes = oldmin;
    } else if (type == "Month") {
       oldmonth++;
       if (oldmonth > 12) {
        oldmonth = 1;
       }
       oldval = oldmonth;
       month = oldmonth;
    } else if (type == "Day") {
        oldday++;
        if (oldday > 31) {
          oldday = 1;
        }
        oldval = oldday;
        dayofmonth = oldday;
    } else if (type == "Year") {
        oldyear++;
        if (oldyear > 50 || oldyear < 17) {
          oldyear = 17;
        }
        oldval = oldyear;
        year = oldyear + 2000;
    }
  }
}
