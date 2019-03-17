# ArduinoDS1302Clock2ButtonMenuSystemOLED  
Arduino with a DS1302 Real Time Clock Module and two buttons for setting time and date.  
Utilizes the SoftTimer library at: https://github.com/prampec/arduino-softtimer  
Original DS1302 code was created by Krodel at: http://playground.arduino.cc/Main/DS1302  
Much of the work for the clock was done via the provided playground sketch.

## Schematic
Find a schematic of the circuit at: in the repo at Arduino_DS1302_RTC_Clock_with_OLED_2_Button.pdf
        ![OLED DS1302 Schematic]
      

## OLED
OLED was a SSD1306 128x64 using the Adafruit SSD1306 library  
### Other Notes
You probably want to use the playground code "as is" to set the initial time once so values aren't empty when you 
start setting the time. I did not test this sketch with a blank DS1302, but if it gives you trouble, just use the
`JUST_THIS_ONCE` flag in the original DS1302 code to set the time first.  
My code is not great, but does work. I'm sure that someone could make it more efficient, and/or improve upon it.
Maybe a rotary encoder, or more buttons, etc...  
I could not get the 12 hr clock settings in the DS1302 code to work, so I ended up doing an `if` statement to 
set the AM/PM variable to display on the OLED.

#### To Do
- I did not impement any checks to verify the Day of the Month is valid for a given month. For example, it is possible
to set 30 days for Feb, or 31 days for March, etc... Perhaps I'll fix in a later release.
 - Add a flag/button to switch from 24hr time to 12hr time.

