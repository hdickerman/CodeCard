// CodeCard Draw
// Copyright (c) 2020 Harold Dickerman
// 

    /**The MIT License (MIT)
    
    Copyright (c) 2020 by Harold Dickerman
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
    */
//
//  Button A changes direction that the cursor will move and the direction is shown in the lower left corner
//  Button B moves cursor in the direction indicated by the number of steps as shown in the lower right corner
//  Pressing both Buttons A and B changes the mode (erase, draw 1 step, draw 2 steps, draw 3 steps, draw 4 steps, or draw 5 steps)
//  The X, Y and number of repeated steps (when holding the B Button) is shown on the bottom
//

#include <pgmspace.h>         // Using PROGMEM for Font
#include <GxEPD2_BW.h>        // Download/Clone and put in Arduino Library https://github.com/ZinggJM/GxEPD2

#include <Fonts/FreeMonoBold9pt7b.h>

#include "icons.h" // Control icons and name to index funcion

// Also make sure you have the Adafruit GFX library

GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> display(GxEPD2_270(/*CS=D8*/ 2, /*DC=D3*/ 0, /*RST=D4*/ 4, /*BUSY=D2*/ 5)); // 2.7" b/w 264x176

#define WAKE_PIN 16
#define BAUD_SPEED 115200

// Button pins
#define BUTTON1_PIN 10   //10
#define BUTTON2_PIN 12    //12
#define LED_PIN 2 // LED Pin (active low)

int btn1State = LOW;
int btn2State = LOW;
int button1Cnt = 0;  // count of button press duration for debounce
int button2Cnt = 0;  // count of button press duration for debounce
bool btn1First = false;  //button 1 pressed before button 2
bool btn2First = false;  //button 2 pressed before button 1

int stepCnt = 0; // count of steps

float x ; // note x & y should be integers, set to floating point to accumulate fractions for movement
float y ; // note x & y should be integers, set to floating point to accumulate fractions for movement
int i;
int ii;
int currentPixel = 0; // color of current pixel

// direction index 0-15 corresponds to 0-337.5 degrees in increments of 22.5 degrees
const int deltaX[] = { 0 , 3 , 5 , 6 , 7 , 6 , 5 , 3 , 0 ,-3 ,-5 ,-6 ,-7 ,-6 ,-5 ,-3 };  // X axis points for direction index
const int deltaY[] = {-7 ,-6 ,-5 ,-3 , 0 , 3 , 5 , 6 , 7 , 6 , 5 , 3 , 0 ,-3 ,-5 ,-6 };  // X axis points for direction index
const float sinDir[] = {0 ,0.382683588 ,0.707107019 ,0.923879726 ,1 ,0.92387921 ,0.707106067 ,0.382682344 ,0 ,-0.382684832 ,-0.707107971 ,-0.923880241 ,-1 ,-0.923878695 ,-0.707105115 ,-0.3826811}; // sin table for direction index
const float cosDir[] = {1 ,0.923879468 ,0.707106543 ,0.382682966 ,0 ,-0.38268421 ,-0.707107495 ,-0.923879983 ,-1 ,-0.923878953 ,-0.707105591 ,-0.382681722 ,0 ,0.382685454 ,0.707108447 ,0.923880499}; // cos table for direction index
const char compassPoint[][4] = {" N ","NNE","N-E","ENE"," E ","ESE","S-E","SSE"," S ","SSW","S-W","WSW"," W ","WNW","N-W","NNW"};

byte direction = 0;
byte controlState = 0;
      
void setup() {
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH); //immediately set wake pin to HIGH to keep the chip enabled
    
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP); 
  pinMode(LED_PIN, INPUT_PULLUP); // LED
  digitalWrite(LED_PIN, LOW); //immediately set wake pin to HIGH to keep LED off
  
  btn1State = digitalRead(BUTTON1_PIN);
  btn2State = digitalRead(BUTTON2_PIN);

  Serial.begin(BAUD_SPEED);
  Serial.println("Starting...");

  display.init();

  helloWorld();

  //wait for any key press
  pinMode(BUTTON2_PIN, INPUT_PULLUP);  // Button 2 = B
  while (digitalRead(BUTTON1_PIN) == 0 && digitalRead(BUTTON2_PIN) == 0)
  {
    yield(); // Nothing to do but wait....BLOCKING code, yield() allows WDT timer to be reset and not stack dump...
  }
 
  //reset and clear screen
  display.setFullWindow();
  display.setRotation(3);
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.nextPage();

  x = display.width()/2;
  y = display.height()/2;
  display.setPartialWindow(0, 0, display.width(), display.height());  // whole screen as partial

  controlState = 1; // write 1 step
  displayControl(); // update screen
  
  direction  = 0; // upwards
  displayDirection(); // update screen
  
}

void loop()
{
    // Speed of loop dictated by speed of display.nextpage()
    {
    // This is all the button logic.....
    // display uses SPI1/HSPI which impacts GPIO12 used by button 2, pinMode line below resets it so it can be read properly
    pinMode(BUTTON2_PIN, INPUT_PULLUP);  // Button 2 = B
    btn1State = digitalRead(BUTTON1_PIN);
    btn2State = digitalRead(BUTTON2_PIN);

    if (btn1State != 0) button1Cnt++; //increment button cycle counter for debouncing buttons
    if (button1Cnt > 2) button1Cnt = 2; // max of 2 cycles
    if (btn2State != 0) button2Cnt++; //increment button cycle counter for debouncing buttons
    if (button2Cnt > 2) button2Cnt = 2; // max of 2 cycles
    if (button1Cnt == 2 && button2Cnt <  2) btn1First = true; // Button 1 pressed before button 2
    if (button1Cnt <  2 && button2Cnt == 2) btn2First = true; // Button 2 pressed before button 1
    if (btn1State == 0) 
    {
      button1Cnt = 0; //reset button press counter if not pressed
      btn1First = false; // reset button pressed first
    }
    if (btn2State == 0) 
    {
      button2Cnt = 0; //reset button press counter if not pressed
      btn1First = false; // reset button pressed first
    }

    // Logic based on button presses....   
    if (button1Cnt > 1 && button2Cnt > 1 ) // both buttons pressed
    {
      // Do stuff when both buttons pressed
      button1Cnt = 1; // partially clear counter to prevent activating of button1 only
      controlState++;
      if (controlState > 5) controlState = 0;  // only erase and write
      displayControl(); // update icon on screen      
    }
    else if (button1Cnt > 1 && button2Cnt == 0 ) // Only A button pressed
    {
      button1Cnt = 1; // partially clear counter
      // Do stuff for only button 1
      direction++;
      if (direction > 15) direction = 0;
      displayDirection(); // update icon on screen
    }
    else if (button1Cnt == 0 && button2Cnt > 1 ) // Only B button pressed
    {
      button2Cnt = 1; // partially clear counter
      // Do stuff for only button 2
      moveCursor();
    }
    else // none of those (no button pressed)
    {
      // nothing to do if nothing pressed
    }

    if (button2Cnt == 0) stepCnt = 0; // reset step count if button 2 was released..
    
  }  // end of button logic
  
  display.drawPixel(int(x), int(y), GxEPD_BLACK);
   
  display.nextPage();
  
}

void helloWorld()
{
  display.setFullWindow();
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.firstPage();

  //Serial.println(FreeMonoBold9pt7b.yAdvance);

  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 0); // set to top of display
  display.println(); // advance to first printable line
  display.println(F("      CodeCardDraw"));
  display.println(F("Btn A changes direction"));
  display.println(F("as shown in left corner"));
  display.println(F("Btn B moves cursor"));
  display.println(F("Btn A and B changes mode"));
  display.println(F("as shown in right corner"));
  display.println(F("Number is advance amount"));
  display.println();
  display.println(F("Press a button to start"));
 
  display.nextPage();

}

void moveCursor()
{
  
  currentPixel = GxEPD_BLACK; // overwrite with black
  if (controlState == 0 ) currentPixel = GxEPD_WHITE; // erase with white

  ii = 1; // iterate at lease once
  if (controlState > 0 ) ii = controlState; // controlState = interations (except erase)

  for (i=0;i < ii; i++)
  {
    display.drawPixel(int(x), int(y), currentPixel );  // set current pixel prior to move

    if (((x + sinDir[direction]) > 0) && ((x + sinDir[direction]) < display.width()))  // in X bounds of display?
    {
      x = x + sinDir[direction];
    }
    if (((y - cosDir[direction]) > 0) && ((y - cosDir[direction]) < display.height())) // in Y bounds of display?
    {
      y = y - cosDir[direction]; // subtraction due to orientation
    }
  }

  stepCnt++; // count times through and display
  // display X,Y and step count
  display.fillRect(80,176-16,264-80-16,16,GxEPD_WHITE); // clear prior info window
  display.setCursor(80,174); 

  display.print("(");
  display.print(int(x));
  display.print(",");
  display.print(int(y));
  display.print(") ");
  display.print(stepCnt);
  display.print("x");
}

void displayControl()
{
  display.fillRect(  264-16 , 176-16,16,16,GxEPD_WHITE); //clear out previons
  display.drawBitmap(264-16 , 176 - 16 , ControlIcon[ controlState ], 16, 16, GxEPD_BLACK );  //  display icon
}

void displayDirection()
{
  display.fillRect(0,176-16,54,16,GxEPD_WHITE); //clear out previons
  display.drawLine(0+8,176-16+8,0+8+deltaX[direction],176-16+8+deltaY[direction],GxEPD_BLACK );  //  display direction line
  display.fillRect(0+8-1,176-16+8-1,3,3,GxEPD_BLACK );  //  draw center square
  display.setCursor(20,174);
  display.print(compassPoint[direction]);
}
