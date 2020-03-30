// CodeCard Clock Display
// Copyright (c) 2019 Harold Dickerman
// 
// EEPROM Portions adapted from https://github.com/shadowedice/StockTicker

    /**The MIT License (MIT)
    
    Copyright (c) 2019 by Harold Dickerman
    
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

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "clockfaces.h"

#include <WiFiClient.h>
char host[100] ;  // character array for hostname
const int httpsPort = 443;

// WIfi
String ssid = "";
String password = "";
String IPaddress = "";
String conn_ssid = "";

// Connection timeout;
#define CON_TIMEOUT   (20*1000)          // milliseconds

#include <pgmspace.h>         // Using PROGMEM for Font
#include <GxEPD2_BW.h>        // Download/Clone and put in Arduino Library https://github.com/ZinggJM/GxEPD2
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

///Date stuff

char *monthNames[] = {"??? ","Jan ","Feb ","Mar ","Apr ","May ","Jun ","Jul ","Aug ","Sept ","Oct ","Nov ","Dec "};
char *DoWNames[] = {"Sun","Mon","Tue","Wed","Thur","Fri","Sat"};  // Sunday = 0
char fullDate[32] = {0}; // full date buffer


const int EEPROM_SIZE = 512;  //Can be max of 4096
const int WIFI_TIMEOUT = 30000; //In ms

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

#define CON_TIMEOUT   20*1000                     // milliseconds

bool connectSuccess = false;

// Time keeping 
int8_t hrs = 1;
int8_t hrs24 = 1;
int8_t mnt = 0;
volatile uint8_t sec = 0;  // volatile as it is used in ISR, single byte for same reason
int lastSec = -1;  // last saved second
unsigned long secondsCount = 0 ; // seconds between update
unsigned long secondsCountLast = 0 ; // seconds between update
unsigned long timeCurLast = 0; // last timeCur from API
unsigned long timeCur;
char currentDate[] = "0000-00-00 ";
int8_t dOW = 0; // day of week (Sun = 0)
char status[32] = {0};
bool timeSync = true; // if time sync is required
bool firstDisplay = false; // first time screen displayed

// Clock lookup
String timeZone = "";  // timeZone e.g. America/new_york

float xx = 0;
float yy = 0;
float xd = 0;
float yd = 0;
int mnt_hand = 75; // length of min hand
int hrs_hand = mnt_hand * 2 / 3; // length of hrs hand
#define X_CENTER 132
#define Y_CENTER 87

bool float_hands = true; // whether hands go all way to center
bool center_point = true ; // whether there is a center point
char end_type = 'N' ;  // End of Hand type
bool dot_hands = true; // whether there is a dot at the end of the clock hands
bool cir_hands = true; // whether there is an open circle at the end of the clock hands
bool arr_hands = true; // whether there is an arrow at the end of the clock hands
bool tri_hands = true; // whether there is an filled triangle at the end of the clock hands
bool show_hands = true; // whether the hands are displayed
bool clock_ticks = true; // whether there are tick marks on clock face
bool clock_face = true; // whether there is a circle clock face
bool trace_hands = true; // whether there is a trace of the path of the hands
int face_style = 0; // style of face


#define RAD_TO_DEGREES   0.0174532925

#include <string.h>
int16_t tbx, tby;
uint16_t tbw, tbh;
uint16_t x, y;
int i = 0;

const char* header = "<h1>CodeCard Analog Clock Configuration</h1><p><form  name='wifi' method='post'>SSID:<input type='text' name='ssid'> Password:<input type='text' name='pass'><p>Full timezone name:<input type='text' name='timezone'><p><input type='checkbox' name='float_hands' value='float_hands'>Float hands<br><input type='checkbox' name='center_point' value='center_point' checked>Center point<br>Hand End type:<br><input type='radio' name='end_type' value='N' checked> None<br><input type='radio' name='end_type' value='A'> Arrows<br><input type='radio' name='end_type' value='C'> Circles<br><input type='radio' name='end_type' value='D'> Dots<br><input type='radio' name='end_type' value='T'> Triangles<br><input type='checkbox' name='show_hands' value='show_hands' checked>Show hands<br><input type='checkbox' name='trace_hands' value='trace_hands'>Trace path of hands<br><input type='checkbox' name='clock_face' value='clock_face' checked>Show clock face<br><input type='checkbox' name='clock_ticks' value='clock_ticks' checked>Ticks on clock face<br>Face style<select name='face_style'><option value='0'>0</option><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option><option value='5'>5</option></select><br><input type='submit' name ='config' value='Configure'></form><p>";
const char* tail = "<p>";


ESP8266WebServer server(80);

void ICACHE_RAM_ATTR onTimerISR() // interrupt to count seconds
{
    //timer1_write(312500); //Reset timer: 312500 ticks * 3.2 us/tick = 1000000 us = 1 sec  only if TIM__SINGLE
    sec++;
    if (sec > 59) sec = 0; // track seconds, rollover @ 60
    //digitalWrite(LED,!(digitalRead(LED)));  //Toggle LED Pin
}


void helloWorld()
{
  const char HelloWorld[] = "Hello World!";
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);

  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  x = ((display.width() - tbw) / 2) - tbx;
  y = ((display.height() - tbh) / 2) - tby;
  display.setCursor(x, y);
  display.print(HelloWorld);

  display.getTextBounds(conn_ssid, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  x = ((display.width() - tbw) / 2) - tbx;
  y = y + 24;
  display.setCursor(x, y);
  display.print(conn_ssid);

  display.getTextBounds(IPaddress.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  x = ((display.width() - tbw) / 2) - tbx;
  y = y + 24;
  display.setCursor(x, y);
  display.print(IPaddress.c_str());

  while (display.nextPage());
  delay(5000);

  // Serial.println("helloWorld done");
}

void setup()
{
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

  // Initial display/Text parameters
  display.init();
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);


  // Serial.println("All done");

  //Read EEPROM to get ssid/password and stock tickers
  readEEPROM();

  //Connect to the wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.persistent(false); // only update flash with SSID/Pass if changed
  WiFi.enableInsecureWEP(); // Allow insecure encryptions
  WiFi.mode(WIFI_STA);
  WiFi.hostname("CCAnalog");  // Sets IP host name of device
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while ( WiFi.status() != WL_CONNECTED && millis() - start  < CON_TIMEOUT ) // couldn't connect, exceeded timeout
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  if ( WiFi.status() == WL_CONNECTED && ssid.length() > 0)  // successful?
  {
    IPaddress = WiFi.localIP().toString();  // Get connected IP
    connectSuccess = true;
    conn_ssid = "SSID: " + ssid; // connected to SSID
    Serial.println("WiFi connected");
  }
  else
  {
    connectSuccess = false;  // Flag station access failed
    WiFi.disconnect(true);  // shut off station access as not compatible with AP mode due to channel switching
    Serial.println("Failed to connect to wifi, Starting AP");

    if (WiFi.softAP("CodeCard_AP") == true)  // try to enable AP for conf and show results
    {
      Serial.println("AP Ready");
      IPaddress = WiFi.softAPIP().toString();   // save AP address
      conn_ssid = "SSID: CodeCard_AP";  // connected to SSID
    }
    else
    {
      Serial.println("AP Failed!");
      IPaddress = "0.0.0.0";      // save no address
    }
  }

  Serial.print("Connected, IP address: ");
  Serial.println(IPaddress);

  // Say Hello to be nice!
  helloWorld(); // Display Hello and show connected IP address
//*/
  //Start the server for the configuration webpage
  server.on("/", HTTP_GET, defaultPage);
  server.on("/", HTTP_POST, response);
  server.begin();

  //  Fewer number of interrupts, the less wasted overhead
  //  TIM_DIV1   = 0  //80MHz (80 ticks/us - 104857.588 us max)
  //  TIM_DIV16  = 1 //5MHz (5 ticks/us - 1677721.4 us max)
  //  TIM_DIV256 = 3 //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
  //     timer1_write(num_ticks); // num_ticks * ticks/us sec = time (us)
  
  timeSync = true; // force initial time sync
  firstDisplay = true; // first time display screen
  lastSec = -1 ; // force lastSec != sec
  noInterrupts(); // critical, time-sensitive code
  sec = 0 ; // reset seconds
  interrupts();  // re-enable interrupts

  //Initialize timer to interrupt every 1 sec (count down to 0) (312500 ticks * 3.2 us/tick = 1000000 us = 1 sec)
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP); // TIM_LOOP = restart with same value, TIM_SINGLE = need to reset value
  timer1_write(312500); //312500 ticks * 3.2 us/tick = 1000000 us = 1 sec, prime timer....

}

void loop() 
{

  while (sec == lastSec) // milliseconds to wait between seconds
  {
    server.handleClient();  // do configuration web page setup
  }

  // This happens once a second
  
  if (lastSec >= 0) // accumlate how long passed (maybe > 1 sec...)
  {
    if (sec > lastSec) secondsCount = secondsCount + sec - lastSec; 
    else               secondsCount = secondsCount + sec + 60 - lastSec ; // account for rollover
  }
  lastSec = sec ; // save last sec
  
  //time keeping stuff, computes mnts and hrs
  // Synchronizes time @ midnight (to get day change) and 2AM (to get time change).
  
  if (sec == 0)  // This happens once a minute
     {
      //sec = sec - 60;
      mnt ++;
      if (mnt >= 60) // rollover?
        {
        mnt = 0;
        hrs24 ++;
        if (hrs24 > 23) hrs24 = 0;
        hrs = hrs24 % 12;
        if (hrs == 0) hrs = 12; 
        }
 
      // do other stuff @ top of minute
      if (connectSuccess == true) // if connected to a valid SSID, get time
      {
        if ((hrs24 == 0 || hrs24 == 2) && mnt == 0 ) timeSync = true ; // update/sync time @  2 or 12
        if (timeSync == true) // update/sync time needed?
        {
          if (timeCur > 0) timeCurLast = timeCur; // if time valid, save last time from API, before getting new time
          timeCur = 0 ;  // clear current time
          
          updateCurrentTime(); // do time update (hrs, mnt, sec, currentDate, dOW)
    
          // calculate drift between CPU time in sec and time from API calls to recalibrate CPU time
          if (timeCur > 0 ) // only do if valid time returned by API
          {
            timeSync = false; // flag time sync completed successfully
            secondsCountLast = secondsCount; // save seconds count since last update time API call successful
            secondsCount = 0; // reset seconds count
          }
    
          if (timeCur > 0 && timeCurLast > 0 ) // only do if valid times current and last
          {
            // calculate drift between CPU time in sec and time from API calls
            Serial.print("Seconds from API calls: ");
            Serial.println(timeCur - timeCurLast);
            Serial.print("Seconds Counted: ");
            Serial.println(secondsCountLast);
            Serial.print("Accuracy: ");
            Serial.println((float)(timeCur - timeCurLast)/secondsCountLast);
          }
       }

       if (sec == 0 || firstDisplay == true)  displaytime(); // display time every minute if connected....(firstDisplay = true first time through)
       firstDisplay = false; // set first display to false
     } // minute and connected
   }  // done with minute stuff
}

void displayerror()
{
        display.setFullWindow();
        display.firstPage();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(25, 88);
        display.print(F("Worldtime API Failure"));
        while (display.nextPage());
        return; // exit  
}

void updateCurrentTime() 
// leverages worldtimeapi.org to get current time based on timezone
// ex: http://worldtimeapi.org/api/timezone/AMERICA/new_york
// Returns:
// ulong timeCur  // time in sec (unix time)
// char currentDate[] = "2019-01-10 ";
// int hrs, mnt, sec for hours, minutes, seconds
// Manages hrs, mnts, sec as well as get currentDate and dOW
{   
  Serial.println("Syncing time..");
  if (timeZone.length() == 0 ) timeZone = "America/new_york";  // default
  strcpy(host,"worldtimeapi.org");
  
  WiFiClient client;

  //Serial.printf("\n[Connecting to %s ... ", host);
  if (client.connect(host, 80))
  {
    //Serial.println("connected]");

    //Serial.println("[Sending a request]");

    //Serial.println("[Sending a request]");
    client.print(F("GET /api/timezone/"));
    client.print(timeZone);
    client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Host: "));
    client.print(host);
    client.print(F("\r\n"));
    client.print(F("Connection: close\r\n\r\n"));
  }

  unsigned long timeout = millis();
  while (client.available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  //Read all the lines of the reply from server and print them to Serial

  memset (status, 0, sizeof(status)); // Clears out old buffer
    
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0)
  {
    Serial.println(status);
    Serial.println("Unexpected HTTP response");
    client.stop();
    return;
  }

  //Skip Headers
  char endOfHeaders[] = "\r\n\r\n";
  client.find(endOfHeaders) || Serial.print("Invalid response");

  //Serial.println(client);  // show full response

  memset (status, 0, sizeof(status)); // Clears out old buffer

  // {"week_number":49,"utc_offset":"-05:00","utc_datetime":"2019-12-07T17:40:29.503451+00:00","unixtime":1575740429,"timezone":"America/New_York","raw_offset":-18000,"dst_until":null,"dst_offset":0,"dst_from":null,"dst":false,"day_of_year":341,"day_of_week":6,"datetime":"2019-12-07T12:40:29.503451-05:00","client_ip":"24.61.79.91","abbreviation":"EST"}

  client.readBytesUntil(':', status, sizeof(status));  // See if valid response by reading up to first :
  //Serial.print("status ");
  //Serial.println(status);
  if (strcmp(status , "{\"week_number\"") == 0) // valid results?  // should have '{"week_number"' if valid
  {
    memset (status, 0, sizeof(status)); // Clears out old buffer
    client.find("\"unixtime\":");
    client.readBytesUntil(',', status, sizeof(status));  // read to comma
    timeCur = strtoul(status, NULL, 0);  /// convert to long int
    //Serial.println(timeCur);

    memset (status, 0, sizeof(status)); // Clears out old buffer
    client.find("\"day_of_week\":");
    client.readBytesUntil(',', status, sizeof(status));  // read to comma
    dOW = atoi(status);  // convert to int

    memset (currentDate, 0, sizeof(currentDate)); // Clears out old buffer
    client.find("\"datetime\":\"");
    client.readBytesUntil('T', currentDate, sizeof(currentDate));  // read to T separating date and time
    //Serial.println(currentDate);

    memset (status, 0, sizeof(status)); // Clears out old buffer
    client.readBytesUntil(':', status, sizeof(status));  // HRS read to :
    hrs24 = atoi(status) ;
    hrs = hrs24 % 12; // initialize hrs too
    if (hrs == 0) hrs = 12; 

    memset (status, 0, sizeof(status)); // Clears out old buffer
    client.readBytesUntil(':', status, sizeof(status));  // MTS read to :
    mnt = atoi(status);
    
    memset (status, 0, sizeof(status)); // Clears out old buffer
    client.readBytesUntil('.', status, sizeof(status));  // SEC read to :

    noInterrupts(); // critical, time-sensitive code
    sec = atoi(status) ; // get seconds
    interrupts();  // re-enable interrupts

    client.print(F("\r\n\r\n"));
  }
  else
  {
    Serial.print("Bad Timezone ");
    Serial.println(timeZone);
  }

  client.stop();
}


void displaytime()
{
// bool float_hands = true; // whether hands go all way to center
// bool center_point = true ; // whether there is a center point
// bool dot_hands = true; // whether there is a dot at the end of the clock hands
// bool cir_hands = true; // whether there is an open circle at the end of the clock hands
// bool arr_hands = true; // whether there is an arrow at the end of the clock hands
// bool tri_hands = true; // whether there is an filled triangle at the end of the clock hands
// bool show_hands = true; // whether the hands are displayed
// bool clock_ticks = true; // whether there are tick marks on clock face
// bool clock_face = true; // whether there is a circle clock face
// bool trace_hands = true; // whether there is a trace of the path of the hands

       //Show time for debugging
      Serial.print(hrs);
      Serial.print(":");
      Serial.print(mnt);
      //Serial.print(":");
      //Serial.print(sec);
      Serial.println("");
      
      // clear screen and create colon
      display.fillScreen(GxEPD_WHITE);
      display.setFont(&FreeMonoBold12pt7b);

      mnt_hand = 75; // length of min hand
      if (face_style > 0 && face_style < 6) 
      {
        display.drawBitmap(45 , 0 , clockface[face_style - 1], 176  , 175, GxEPD_BLACK );  // load bitmap
        mnt_hand = 60; // make length of min hand shorter
      }
      hrs_hand = mnt_hand * 3 / 4; // length of hrs hand based on min hand

      // draw clock face
      if (clock_face) display.drawCircle(X_CENTER,Y_CENTER,87,GxEPD_BLACK); // Outer circle 264x176 display

      // draw tick marks
      if (clock_ticks)
      {
        for (i = 0; i < 360; i = i + 30)
        {
          xx = sin((float)(i * RAD_TO_DEGREES ));
          yy = cos((float)(i * RAD_TO_DEGREES ));
          display.drawLine  (X_CENTER + (xx * 83), Y_CENTER - (yy * 83), X_CENTER + (xx * 87),Y_CENTER - (yy * 87), GxEPD_BLACK);
        }
      }
      
      // Calc Minute Hand
      xx = sin((float) ((mnt * 6 ) * RAD_TO_DEGREES) ) * mnt_hand;  // 6 degrees per minute
      yy = cos((float) ((mnt * 6 ) * RAD_TO_DEGREES) ) * mnt_hand;  // 6 degrees per minute
      if (show_hands) display.drawLine  (X_CENTER,Y_CENTER ,X_CENTER + xx,Y_CENTER - yy, GxEPD_BLACK);

      // Minute hand dot
      if (dot_hands) display.fillCircle(X_CENTER + xx, Y_CENTER - yy ,4,GxEPD_BLACK);  // Solid Center 264x176 display
      if (cir_hands) display.fillCircle(X_CENTER + xx, Y_CENTER - yy ,4,GxEPD_WHITE);  // Clear circle Solid Center 264x176 display
      if (cir_hands) display.drawCircle(X_CENTER + xx, Y_CENTER - yy ,4,GxEPD_BLACK);  // draw outline Solid Center 264x176 display
      if (arr_hands) 
      { // Arrow hands.... 
        display.drawLine  (X_CENTER + xx,Y_CENTER - yy, X_CENTER + xx - (sin((float) (((mnt * 6 ) -45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) (((mnt * 6) -45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
        display.drawLine  (X_CENTER + xx,Y_CENTER - yy, X_CENTER + xx - (sin((float) (((mnt * 6 ) +45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) (((mnt * 6) +45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
      }
      if (tri_hands) 
      { 
        // Triangle hands.... 
        display.fillTriangle (X_CENTER + xx,Y_CENTER - yy
          , X_CENTER + xx - (sin((float) (((mnt * 6 ) -45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) (((mnt * 6) -45 ) * RAD_TO_DEGREES) ) * 8)
          , X_CENTER + xx - (sin((float) (((mnt * 6 ) +45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) (((mnt * 6) +45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
      }

      // Calc Hour Hand
      xx = sin((float) (((hrs * 30) + (float)(mnt / 2)) * RAD_TO_DEGREES)) * hrs_hand; // 30 degrees per hour + .5 degrees per min
      yy = cos((float) (((hrs * 30) + (float)(mnt / 2)) * RAD_TO_DEGREES)) * hrs_hand; // 30 degrees per hour + .5 degrees per min
      if (show_hands) display.drawLine(X_CENTER,Y_CENTER,X_CENTER + xx,Y_CENTER - yy, GxEPD_BLACK);

      // Hour hand dot
      if (dot_hands) display.fillCircle(X_CENTER + xx, Y_CENTER - yy ,6,GxEPD_BLACK);  // Solid Center 264x176 display
      if (cir_hands) display.fillCircle(X_CENTER + xx, Y_CENTER - yy ,6,GxEPD_WHITE);  // Clear circle Solid Center 264x176 display
      if (cir_hands) display.drawCircle(X_CENTER + xx, Y_CENTER - yy ,6,GxEPD_BLACK);  // draw outline Solid Center 264x176 display
      if (arr_hands) 
      { // Arrow hands.... 
        display.drawLine  (X_CENTER + xx,Y_CENTER - yy, X_CENTER + xx - (sin((float) ((((hrs * 30) + (float)(mnt / 2) ) -45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) ((((hrs * 30) + (float)(mnt / 2)) -45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
        display.drawLine  (X_CENTER + xx,Y_CENTER - yy, X_CENTER + xx - (sin((float) ((((hrs * 30) + (float)(mnt / 2) ) +45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) ((((hrs * 30) + (float)(mnt / 2)) +45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
      }
      if (tri_hands) 
      { 
        // Triangle hands.... 
        display.fillTriangle (X_CENTER + xx,Y_CENTER - yy
          , X_CENTER + xx - (sin((float) ((((hrs * 30) + (float)(mnt / 2) ) -45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) ((((hrs * 30) + (float)(mnt / 2)) -45 ) * RAD_TO_DEGREES) ) * 8)
          , X_CENTER + xx - (sin((float) ((((hrs * 30) + (float)(mnt / 2) ) +45) * RAD_TO_DEGREES) ) * 8) ,Y_CENTER - yy + (cos((float) ((((hrs * 30) + (float)(mnt / 2)) +45 ) * RAD_TO_DEGREES) ) * 8),GxEPD_BLACK);
      }

      // draw trail
      if (trace_hands)
      {
        for (i = 0; i < 360*2; i++)  // twice the resolution
        {
          xx = sin((float)(i * RAD_TO_DEGREES / 2));
          yy = cos((float)(i * RAD_TO_DEGREES / 2));
          if (i/2 <= (mnt * 6 ))                       display.drawLine (X_CENTER + (xx * (hrs_hand+4)), Y_CENTER - (yy * (hrs_hand+4)), X_CENTER + (xx * mnt_hand),Y_CENTER - (yy * mnt_hand), GxEPD_BLACK); // draw from end of hrs hand+3 to end of min hand
          if (i/2 <= ((hrs * 30) + (float)(mnt / 2)) ) display.drawLine (X_CENTER , Y_CENTER , X_CENTER + (xx * hrs_hand),Y_CENTER - (yy * hrs_hand), GxEPD_BLACK); // draw from center to end of hrs hand
        }
      }

      // make hands floating by drawing white circle in middle
      if (float_hands) display.fillCircle(X_CENTER,Y_CENTER,(hrs_hand/2),GxEPD_WHITE);  // Floating hands 264x176 display

      // Center circle
      if (center_point) display.fillCircle(X_CENTER,Y_CENTER,4,GxEPD_BLACK);  // Solid Center 264x176 display

      display.setFont(&FreeMonoBold9pt7b); // set font
     
      // Put Day of Week on left
      display.setCursor(0, 173);
      display.print(DoWNames[dOW]);

      //                  0123456789
      // currentDate[] = "2019-01-10 ";
      // Build date into char array
      display.setCursor(264 - 66, 173);  // 7 characters, each character is 11 px wide
      display.print(monthNames[((int)currentDate[5]-48)*10 + ((int)currentDate[6]-48)]);  // strcpy for first part re-initializes buffer
      if (currentDate[8] > '0') // day > 9?
      {
        display.print(currentDate[8]); // day (tens)
      }
      display.print(currentDate[9]); // day (ones)

      while (display.nextPage());
   
}


// Config Webpage stuff...
void defaultPage() 
{
  webpage("");
}

void webpage(String status) 
{
  server.send(200, "text/html", header + status + tail );
}

void response() 
{
  if (server.hasArg("ssid") && (server.arg("ssid").length() > 0) && server.hasArg("pass") && (server.arg("pass").length() > 0) )
  {
    ssid = server.arg("ssid");
    password = server.arg("pass");
    writeEEPROM();
  }
  if (server.arg("timezone").length() > 0)
  {
    timeZone = server.arg("timezone");
    writeEEPROM();
  }
  float_hands = false;
  if (server.arg("float_hands").length() > 0)
  {
    float_hands = true;
    writeEEPROM();
  }
  center_point = false;
  if (server.arg("center_point").length() > 0)
  {
    center_point = true;
    writeEEPROM();
  }
  if (server.arg("end_type").length() > 0)
  {
    dot_hands = false;
    cir_hands = false;
    arr_hands = false;
    tri_hands = false;
    if (server.arg("end_type") == "N") {} // None
    else if (server.arg("end_type") == "A") arr_hands = true;
    else if (server.arg("end_type") == "C") cir_hands = true;
    else if (server.arg("end_type") == "D") dot_hands = true;
    else if (server.arg("end_type") == "T") tri_hands = true;
    end_type = server.arg("end_type").charAt(0);
Serial.print("@C ");
Serial.println(server.arg("end_type"));
Serial.print("@D ");
Serial.println(end_type);
    writeEEPROM();
  }
  show_hands = false;
  if (server.arg("show_hands").length() > 0)
  {
    show_hands = true;
    writeEEPROM();
  }
  trace_hands = false;
  if (server.arg("trace_hands").length() > 0)
  {
    trace_hands = true;
    writeEEPROM();
  }
  clock_face = false;
  if (server.arg("clock_face").length() > 0)
  {
    clock_face = true;
    writeEEPROM();
  }
  clock_ticks = false;
  if (server.arg("clock_ticks").length() > 0)
  {
    clock_ticks = true;
    writeEEPROM();
  }
  if (server.arg("face_style").length() > 0)
  {
    face_style = server.arg("face_style").toInt() ;
    writeEEPROM();
  }
  webpage("<p><font color=\"green\">Successfully updated SSID/password/timezone/configuration</font></p>");
  displaytime();
}

void readEEPROM() // Data: SSID, Password
{
  String charString = "";
  int phase = 0;
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++)
  {
    char tmp = EEPROM.read(i);
    if (phase == 0 && tmp == NULL)
    {
      ssid = charString; // set ssid
      charString = "";
      phase++;
    }
    else if (phase == 1 && tmp == NULL)
    {
      password = charString;  // set password
      charString = "";
      phase++;
    }
    else if (phase == 2 && tmp == NULL)
    {
      timeZone = charString;  // timeZone
      charString = "";
      phase++;
    }
    else if (phase == 3 && tmp == NULL)
    {
      float_hands = false;
      if (charString == "1") float_hands = true;
      charString = "";
      phase++;
    }
    else if (phase == 4 && tmp == NULL)
    {
      center_point = false;
      if (charString == "1") center_point = true;
      charString = "";
      phase++;
    }
    else if (phase == 5 && tmp == NULL)
    {
      dot_hands = false;
      cir_hands = false;
      arr_hands = false;
      tri_hands = false;
      if (charString == "N") {} // None
      else if (charString == "A") arr_hands = true;
      else if (charString == "C") cir_hands = true;
      else if (charString == "D") dot_hands = true;
      else if (charString == "T") tri_hands = true;
      end_type = charString[0];
      charString = "";
      phase++;
    }
    else if (phase == 6 && tmp == NULL)
    {
      show_hands  = false;
      if (charString == "1") show_hands  = true;
      charString = "";
      phase++;
    }
    else if (phase == 7 && tmp == NULL)
    {
      clock_ticks  = false;
      if (charString == "1") clock_ticks  = true;
      charString = "";
      phase++;
    }
    else if (phase == 8 && tmp == NULL)
    {
      clock_face  = false;
      if (charString == "1") clock_face  = true;
      charString = "";
      phase++;
    }
    else if (phase == 9 && tmp == NULL)
    {
      trace_hands  = false;
      if (charString == "1") trace_hands  = true;
      charString = "";
      phase++;
    }
    else if (phase == 10 && tmp == NULL)
    {
      face_style = int(charString[0]);
      charString = "";
      phase++;
    }
    else if (phase == 11 && tmp == NULL)
    {
      break;
    }
    else
      charString += tmp;
  }
}

void writeEEPROM()  // Data: SSID, Password, apikey, latitude, longitude, offset
{
  int EEPROMAddr = 0;
  //Write out SSID
  for (int i = 0; i < ssid.length(); i++)
  {
    EEPROM.write(EEPROMAddr, ssid.charAt(i));
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  //Write out password
  for (int i = 0; i < password.length(); i++)
  {
    EEPROM.write(EEPROMAddr, password.charAt(i));
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  //Write out timezone
  for (int i = 0; i < String(timeZone).length(); i++)
  {
    EEPROM.write(EEPROMAddr, String(timeZone).charAt(i));
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (float_hands == true) EEPROM.write(EEPROMAddr, '1');
    else                     EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (center_point == true) EEPROM.write(EEPROMAddr, '1');
    else                      EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    EEPROM.write(EEPROMAddr, end_type);
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (show_hands == true)  EEPROM.write(EEPROMAddr, '1');
    else                     EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (clock_ticks == true)  EEPROM.write(EEPROMAddr, '1');
    else                      EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (clock_face == true)  EEPROM.write(EEPROMAddr, '1');
    else                     EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    if (trace_hands == true)  EEPROM.write(EEPROMAddr, '1');
    else                      EEPROM.write(EEPROMAddr, '0');
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  {
    EEPROM.write(EEPROMAddr, face_style);
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  // fill remainder
  while (EEPROMAddr < EEPROM_SIZE)  // fill rest with nulls
  {
    EEPROM.write(EEPROMAddr, NULL);
    EEPROMAddr++;
  }
  EEPROM.commit();
}
