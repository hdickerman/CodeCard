//CodeCard Weather Display
//Leverages DarkSky API.  Need to register for free API Key at https://darksky.net/dev
// Weather is provided based on the latitude and longitude.of the desired location.
// The latitude and longitude for a given location can be determied via the use of maps.google.com
// Use Google Maps to map the desired location.  In the resulting URL the 2 numbers after the @ sign
// are the location's latitude and longitude.  Boston MA is at 42.3550483,-71.0656512 
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

#include "SAXJsonParser.h" // Custom JSON parser
#include "weathericons.h" // weather icons and name to index funcion

// WIfi
String ssid = "";
String password = "";
String IPaddress = "";
String conn_ssid = "";

String apikey = "";
String latitude = "";
String longitude = "";
int offset = 0;


// Connection timeout;
#define CON_TIMEOUT   (20*1000)          // milliseconds

char host[100] = "api.darksky.net";  // character array for hostname
int httpsPort = 443;

#include <pgmspace.h>         // Using PROGMEM for Font
#include <GxEPD2_BW.h>        // Download/Clone and put in Arduino Library https://github.com/ZinggJM/GxEPD2
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

//Date processing
#include "Time.h"
#include "TimeLib.h"

#include <string.h>
int16_t tbx, tby;
uint16_t tbw, tbh;
uint16_t x, y;

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

///weather variables
int tempCur    = 0;
int windSpeed = 0;
int bearing = 0;
char wIconCur[20];
unsigned long currentTime[6];
int tempHi[6] = {0,0,0,0,0,0} ;
int tempLo[6] = {0,0,0,0,0,0} ;
char wIcon[6][20];
int precip[6] = {0,0,0,0,0,0} ;
char precipType[6][10];
int wday = - 1;
#define w_update_freq (60 * 60) // Weather update frequency in seconds.  DarkSki limits free to 1000 checks/day or 1 every 1.5 min.

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

float tempFloat = 0; // temporary float for calc

const char* header = "<h1>CodeCard Weather Forecast Configuration</h1><p><form  name='wifi' method='post'>SSID:<input type='text' name='ssid'> Password:<input type='text' name='pass'><p>DarkSky API key:<input type='text' name='apikey'><p>Latitude:<input type='text' name='latitude'><p>Longitude:<input type='text' name='longitude'><p><input type='submit' name ='config' value='Configure Wifi'></form><p>";
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
  WiFi.hostname("CCWeather");  // Sets IP host name of device
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
  lastSec = sec ; // save last sec
  
  //time keeping stuff, computes mnts and hrs
  // Synchronizes time @ midnight (to get day change) and 2AM (to get time change).
  
  if (lastSec == 0)  // This happens once a minute
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
        if ( mnt % 15 == 0 ) timeSync = true ; // update/sync weather every 15 min
        if (timeSync == true) // update/sync weather needed?
        {
          if (timeCur > 0) timeCurLast = timeCur; // if time valid, save last time from API, before getting new time
          timeCur = 0 ;  // clear current time
          
          updateweather(); // do weather update
    
          // calculate drift between CPU time in sec and time from API calls to recalibrate CPU time
          if (timeCur > 0 ) // only do if valid time returned by API
          {
            // reset clock to API time
            noInterrupts(); // critical, time-sensitive code
            sec = second(timeCur);
            interrupts();  // re-enable interrupts
            
            mnt = minute(timeCur);
            hrs24 = hour(timeCur  - hour(currentTime[0])*3600 );  // hrs adjusted by currentTime which is always midnight local
            if (hrs24 >= 2) hrs24 = hrs24 + 24 - ((currentTime[1] - currentTime[0])/3600);  // hrs adjusted for DST 
            /// based on length in hrs between today and tomorrow (normally 24hrs, but 23hrs in spring DST, and 25hrs in winter DST)
            hrs = hrs24 % 12;
            if (hrs == 0) hrs = 12; 

            timeSync = false; // flag time sync completed successfully
          }
       }

       if (lastSec == 0 || firstDisplay == true)  displayweather(); // display every minute if connected....(firstDisplay = true first time through)
       firstDisplay = false; // set first display to false
     } // minute and connected
   }  // done with minute stuff
}

void updateweather()

// Leverages DarkSky API.  Need to register for free API Key
// Below can be replaced to call to other weather service APIs such as openweathermap.org or www.weatherapi.com
// Darksky has a 'Current weather' and daily forecast (including current day)  For arrays, current day = 0
// Darksky also provides current time which is used to synchronize the time keeping function.
// Uses custom JSON parser which only parses a branch of a JSON tree at a time into key values and data

{
  Serial.println("Updating Weather");

  char c;  
  parse(0);  // initialize parser

  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  client.setInsecure();
  
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  // /forecast/{API Key}/42.1225,-71.1729?exclude=minutely,hourly,alerts,flags
  client.print(F("GET "));
  // Build URL
  client.print(F("/forecast/"));
  client.print(apikey);
  client.print(F("/"));
  client.print(latitude);
  client.print(F(","));
  client.print(longitude);
  client.print(F("?exclude=minutely,hourly,alerts,flags"));
  // URL
  client.print(F(" HTTP/1.1\r\nHost: "));
  client.print(host);
  client.print(F("\r\nUser-Agent: CodeCard\r\nConnection: close\r\n\r\n"));

  //Read response header
  memset (status, 0, sizeof(status)); // Clears out old buffer
    
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0)
  {
    Serial.println(status);
    Serial.println("Unexpected HTTP response");
    client.stop();
    return;
  }

  Serial.println("Darksky request sent");
  while (client.connected()) 
  {
    c = client.read();   // get a character

  //for (long i = 0; i < sizeof(json); i++) /// for testing pull from sample data to test parser
  //{
  //  c = pgm_read_byte_near(json + i);

    if (c != 255)
    {
      parse(c); // process a character
  
      // Decode results from JSON parser
      if (return_type == RETURN_DATA)  // Was return data? (otherwise keep parsing)
      // Decode of JSON tree work is all below
      {       
        if      (strcmp(key_value[1],"currently") == 0)  // get some data from currently section of json tree
        {
          if      (strcmp(key_value[2],"time") == 0)        timeCur = atoi(return_value) ; // Current time
          else if (strcmp(key_value[2],"icon") == 0)        strcpy(wIconCur,return_value);  // icon
          else if (strcmp(key_value[2],"temperature") == 0) tempCur = atof(return_value) + .5 ; // Current temp, rounded
          else if (strcmp(key_value[2],"windSpeed") == 0)   windSpeed = atoi(return_value) ; // Current wind Speed
          else if (strcmp(key_value[2],"windBearing") == 0) bearing = atoi(return_value) ; // Current wind bearing
        }
        else if (strcmp(key_value[1],"daily") == 0 )// get rest of data from daily section of json tree,
        {
          if      (strcmp(key_value[2],"summary") == 0) wday = - 1 ; // reset day counter when summary segment of json tree
          else if (strcmp(key_value[2],"data") == 0 && wday < 6 ) // data segment of json tree for only 5 days wday = 0 is today
          {
            if      (strcmp(key_value[4],"time") == 0)  // time indicates start of data for a day
            {
              wday = wday + 1; // start of new day data
              if (wday < 6)  // only get first 6 days (today and next 5 days) (0-5)
              {
                currentTime[wday] = atoi(return_value);   // save time
              }
            }
            else if (strcmp(key_value[4],"icon") == 0)              strcpy(wIcon[wday],return_value);  // icon
            else if (strcmp(key_value[4],"temperatureHigh") == 0)   tempHi[wday] = atof(return_value) + .5 ;  // temp high, rounded
            else if (strcmp(key_value[4],"temperatureLow") == 0)    tempLo[wday] = atof(return_value) + .5 ;  // temp low, rounded
            else if (strcmp(key_value[4],"precipProbability") == 0) precip[wday] = 100 * atof(return_value) + .5 ; // precip prob, rounded
            else if (strcmp(key_value[4],"precipType") == 0)        strcpy(precipType[wday] , return_value); // precip type
          }
        }
      } // Decode JSON data work is all above
    }
  }
  Serial.print(charCount);
  Serial.println(" Characters read");
  Serial.println("All Done");

}

void displayweather()
{
      Serial.print(hrs);
      Serial.print(":");
      Serial.print(mnt);
      Serial.print(":");
      Serial.print(sec);
      Serial.println("");
    
      if (charCount < 8000)  // API failure
      {
        display.setFullWindow();
        display.firstPage();
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(25, 88);
        display.print(F("Darksky API Failure"));
        while (display.nextPage());
        return; // exit 
      }

      display.setFullWindow();
      display.firstPage();
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&FreeMonoBold9pt7b); 
      
      // Display current time

      y = 14;

      offset = 0 - hour(currentTime[0]); // time of Day 0 always at 12:00AM
      
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(0, y ); // y = bottom of line
      display.print(hrs);
      display.print(F(":"));
      display.print((int) mnt/10); // tens
      display.print( mnt % 10 ); // ones

      // put date on right hand side
      display.setCursor(150, y ); // y = bottom of line
      display.print(month(currentTime[0]));
      display.print(F("/"));
      display.print(day(currentTime[0]));
      display.print(F("/"));
      display.print(year(currentTime[0]));
 
      // Display Day weather data
      
      y = y + 2 ;    
      // Current Weather icon
      display.drawBitmap((264-48)/2 , y , WeatherIcon[ icon2Index(wIconCur) ], 48  , 48, GxEPD_BLACK );  // Weather icon y = Top of icon

      // Current temperature
      display.setFont(&FreeMonoBold18pt7b);
      display.setCursor(0, 54 ); // y = bottom of line
      if (tempCur < 100 && tempCur >= 0) display.print(' ');
      display.print(tempCur);

      y = 42;
      // Current HiTemp for day
            display.setFont(&FreeMono9pt7b);
            display.setCursor(70 , y ); // y = bottom of line
            if (tempHi[0] < 100 && tempHi[0] >= 0) display.print(' ');
            if (tempHi[0] < 10 && tempHi[0] > -10) display.print(' ');
            display.print(tempHi[0]);
      y = y + 14;
    
      // Current LoTemp for day
            display.setCursor(70, y ); // y = bottom of line
            if (tempLo[0] < 100 && tempLo[0] >= 0  ) display.print(' ');
            if (tempLo[0] < 10 && tempLo[0] > -10) display.print(' ');
            display.print(tempLo[0]);
      y = y + 14;

      y = 42; 
      // Current windspeed & bearing
      if (windSpeed > 0) // Is there a wind?
      {
        display.setCursor(160, y ); // y = bottom of line      
        if      (bearing <   22)                   display.print(F("N at "));
        else if (bearing >=  22 and bearing <  67) display.print(F("NE at "));
        else if (bearing >=  67 and bearing < 112) display.print(F("E at "));
        else if (bearing >= 112 and bearing < 157) display.print(F("SE at"));
        else if (bearing >= 157 and bearing < 202) display.print(F("S at "));
        else if (bearing >= 202 and bearing < 247) display.print(F("SW at "));
        else if (bearing >= 247 and bearing < 292) display.print(F("W at "));
        else if (bearing >= 292 and bearing < 337) display.print(F("NW at "));
        else if (bearing >= 337)                   display.print(F("N at "));
  
        display.print(windSpeed);
      }
      
      y = y + 14;

      /// Get current precipType if exists in data...

      if (strlen(precipType[0]) > 0) // is there precipitiation?   Key might not exist!!!!, if so, print precipt %
      {
        display.setCursor(160, y ); // y = bottom of line 
        precipType[0][0] = precipType[0][0] & ~(0x20) ;  // make first letter uppercase by masking bit 5 (32)
        display.print(precipType[0]);
        display.setCursor(display.getCursorX()+5, y ); // y = bottom of line // 1/2 space
        display.print(precip[0]);
        display.print("%");
      }


    // Display forecast for next 5 days.....
    for (int i = 0; i < 5; i++)  // loop for 5 days (day 0 = current day and not shown)
    {

      int y = 80 ;
    
      // Day of week header
            display.setFont(&FreeMonoBold9pt7b);
            display.setCursor((i * 53) + 9 , y );  // y = bottom of line
            switch (weekday(currentTime[i+1])) 
      {
                case 1: display.print(F("SUN"));    break;
                case 2: display.print(F("MON"));    break;
                case 3: display.print(F("TUE"));    break;
                case 4: display.print(F("WED"));    break;
                case 5: display.print(F("THU"));    break;
                case 6: display.print(F("FRI"));    break;
                case 7: display.print(F("SAT"));    break;
            }
      y = y + 4 ;

      // Weather icon
      display.drawBitmap(2 + (i *53) , y , WeatherIcon[ icon2Index(wIcon[i+1]) ], 48  , 48, GxEPD_BLACK );  // Weather icon y = Top of icon
      y = y + 48 ;

      y = y + 12 ; 
      // precipitation percent
            display.setFont(&FreeMono9pt7b);
            display.setCursor((i *53) , y ); // y = bottom of line
            if (precip[i+1] < 100) display.print(' ');
            if (precip[i+1] < 10) display.print(' ');
            display.print(precip[i+1]);
            display.print(F("%"));
      y = y + 14;
    
      // HiTemp
            display.setFont(&FreeMonoBold9pt7b);
            display.setCursor((i *53)  , y ); // y = bottom of line
            if (tempHi[i+1]> 99 || tempHi[i+1] < -9  ) display.print(' ');
            if (tempHi[i+1] < 100 && tempHi[i+1] >= 0) display.print(' ');
            if (tempHi[i+1] < 10 && tempHi[i+1] > -10) display.print(' ');
            display.print(tempHi[i+1]);
      y = y + 14;

      // LoTemp
            display.setCursor((i *53) , y ); // y = bottom of line
            if (tempLo[i+1] > 99 || tempLo[i+1] < -9  ) display.print(' ');
            if (tempLo[i+1] < 100 && tempLo[i+1] >= 0  ) display.print(' ');
            if (tempLo[i+1] < 10 && tempLo[i+1] > -10) display.print(' ');
            display.print(tempLo[i+1]);
      y = y + 14;

    }

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
  if (server.hasArg("config")) 
  {
    if (server.arg("ssid").length() > 0 && (server.arg("pass").length() > 0) )
    {
      ssid = server.arg("ssid");
      password = server.arg("pass");
    }
    if (server.arg("apikey").length() > 0)
    {
      apikey = server.arg("apikey");
    }
    if (server.arg("latitude").length() > 0)
    {
      latitude = server.arg("latitude");
    }
    if (server.arg("longitude").length() > 0)
    {
      longitude = server.arg("longitude");
    }
    writeEEPROM();
    webpage("<p><font color=\"green\">Successfully updated configuration</font></p>");
  }

}

void readEEPROM() // Data: SSID, Password, apikey, latitude, longitude, offset
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
      apikey = charString;  // Darksky API key
      charString = "";
      phase++;
    }
    else if (phase == 3 && tmp == NULL)
    {
      latitude = charString;  // latitude
      charString = "";
      phase++;
    }
    else if (phase == 4 && tmp == NULL)
    {
      longitude = charString;  // longitude
      charString = "";
      phase++;
    }
    else if (phase == 5 && tmp == NULL)
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
  //Write out apikey
  for (int i = 0; i < String(apikey).length(); i++)
  {
    EEPROM.write(EEPROMAddr, String(apikey).charAt(i));
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  //Write out latitude
  for (int i = 0; i < String(latitude).length(); i++)
  {
    EEPROM.write(EEPROMAddr, String(latitude).charAt(i));
    EEPROMAddr++;
  }  
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;
  //Write out longitude
  for (int i = 0; i < String(longitude).length(); i++)
  {
    EEPROM.write(EEPROMAddr, String(longitude).charAt(i));
    EEPROMAddr++;
  }  
  EEPROM.write(EEPROMAddr, NULL);  // terminator
  EEPROMAddr++;

  while (EEPROMAddr < EEPROM_SIZE)  // fill rest with nulls
  {
    EEPROM.write(EEPROMAddr, NULL);
    EEPROMAddr++;
  }
  EEPROM.commit();
}
