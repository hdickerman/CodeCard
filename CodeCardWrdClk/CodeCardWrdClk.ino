// CodeCard Clock Display - Word Clock
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
#include <Fonts/FreeMonoBold18pt7b.h>

///Date stuff

const char *monthNames[] = {" ","January ","February ","March ","April ","May ","June ","July ","August ","September ","October ","November ","December "};
const char *DoWNames[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday = 0
const char *numberNames[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"
    , "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
    , "twenty", "thirty", "fourty", "fifty", "sixty", "seventy", "eighty", "ninety"  // index 20-27
    , "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth", "ninth", "tenth"
    , "twentieth", "thirtieth", "fourtieth", "fiftieth", "sixtieth", "seventieth", "eightieth", "ninetieth" }; // index 28-
char fullDate[32] = {0}; // full date buffer
char buffer[200] = {0};

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

int16_t tbx, tby;
uint16_t tbw, tbh;
uint16_t x, y, xx, yy;
int i = 0;
int irow = 0;
int ii = 0;

#define charW 14
#define charH 14
#define line_length (264/charW)

const char* header = "<h1>CodeCard Word Clock Configuration</h1><p><form  name='wifi' method='post'>SSID:<input type='text' name='ssid'> Password:<input type='text' name='pass'><p>Full timezone name:<input type='text' name='timezone'><p><input type='submit' name ='config' value='Configure'></form><p>";
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

// function to split char array to fit display width
// https://forum.arduino.cc/index.php?topic=267449.0
const char * split (const char * s, const int length)
{
  // if it will fit return whole thing
  if (strlen (s) <= length)
    return s + strlen (s);

  // searching backwards, find the last space
  for (const char * space = &s [length]; space != s; space--)
    if (*space == ' ')
      return space;
   
  // not found? return a whole line
  return &s [length];       
} // end of split

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
  WiFi.hostname("CCWrdClk");  // Sets IP host name of device
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
      //Show time for debugging
      Serial.print(hrs);
      Serial.print(":");
      Serial.print(mnt);
      //Serial.print(":");
      //Serial.print(sec);
      Serial.println("");
      
      // clear screen 
      display.fillScreen(GxEPD_WHITE);
      display.setFont(&FreeMonoBold12pt7b);  // 14x14
      display.setTextColor(GxEPD_BLACK);

      strcpy(buffer,"It's now ");
      strcat(buffer,numberNames[hrs]);  // hrs
      strcat(buffer," ");

      // minutes
      if (mnt > 19) // above 20? Number format is compound
      {
        strcat(buffer,numberNames[18 + mnt/10]);  // 20,30,40,50 start at position 20
        if ((mnt % 10) != 0)  // are there one's digits to add?
        {
          strcat(buffer," ");
          strcat(buffer,numberNames[mnt % 10]);
        }
      }
      else // special cases where min < 20
      {
        if (mnt == 0) strcat(buffer,"o'clock"); // super special case for top of hour
        else if (mnt < 10) // less then 10 need to add 'zero'
        {
          strcat(buffer,numberNames[0]); // add in zero
          strcat(buffer," ");
        }
        if (mnt > 0) strcat(buffer,numberNames[mnt]); // all good 1 - 19
      }

      strcat(buffer," in the ");
      if (hrs24 <  12)               strcat(buffer,"morning");
      if (hrs24 >= 12 && hrs24 < 18) strcat(buffer,"afternoon");
      if (hrs24 >= 18)               strcat(buffer,"evening");
      
      strcat(buffer," on ");

      // day of week
      strcat(buffer, DoWNames[dOW] ); 
      strcat(buffer,", ");

      // Month and year..
      //                  0123456789
      // currentDate[] = "2019-01-10 ";
      // Build date into char array
      // Month name
      strcat(buffer, monthNames[((int)currentDate[5]-48)*10 + ((int)currentDate[6]-48)]);
      // don't need a space here there is one already on the month

      // Day: first do tens....
      i = ((int)currentDate[8]-48);  // tens
      if (currentDate[9] == '0' && i > 1)  strcat(buffer,numberNames[37 - 2 + i]);  // if ones = 0, special case: need to add 20th, 30th, 40th.... start at position 37
      if (currentDate[9] >  '0' && i > 1)  strcat(buffer,numberNames[20 - 2 + i]);  // Add 20,30,40,50 start at position 20

      if (buffer[strlen(buffer) - 1] != ' ') strcat(buffer," ");  // add a space if not already one at end
      
      // Day: next do ones...
      i = ((int)currentDate[9]-48);  // ones
      if (currentDate[8] == '1' && i > 0 ) // tens == 1, case of 10th, 11th, 12th, 13th,... start at position 10
      {
        strcat(buffer,numberNames[10 + i]);  // add 11, 12, 13... starting at position 11
        strcat(buffer,"th"); // add 'th'
      }
      else if (i > 0) strcat(buffer,numberNames[28 - 1 + i]);  // 1st, 2nd, 3rd,... start at position 28

      strcat(buffer,", ");  // add a space if not already one at end

      // do year as 2 2 digit numbers
      // first do tens....
      i = ((int)currentDate[0]-48);  // tens
      if      (i >  1) strcat(buffer,numberNames[20 - 2 + i]);  // Add 20,30,40,50 start at position 20
      else if (i == 0) strcat(buffer,numberNames[0]); // add zero
      
      if (i != 1 && currentDate[1] != '0') strcat(buffer," ");  // add a space if not 1 or 20, 30, 40, ..

      // next do ones...
      i = ((int)currentDate[1]-48);  // ones
      if (currentDate[1] == '1') i = i + 10; // special case for 10, 11, 12, 13...
      if (i > 0) strcat(buffer,numberNames[i]);  // add 1, 2, ... 9, 10, ... 19
      if (currentDate[0] == '0' && i == 0 ) strcat(buffer,numberNames[0]); // add zero if not 20, 30, 40, ...

      if (buffer[strlen(buffer) - 1] != ' ') strcat(buffer," ");  // add a space if not already one at end

      // do year as 2 2 digit numbers
      // first do tens....
      i = ((int)currentDate[2]-48);  // tens
      if      (i >  1) strcat(buffer,numberNames[20 - 2 + i]);  // Add 20,30,40,50 start at position 20
      else if (i == 0) strcat(buffer,numberNames[0]); // add zero
      
      if (i != 1 && currentDate[3] != '0') strcat(buffer," ");  // add a space if not 1 or 20, 30, 40, ..

      // next do ones...
      i = ((int)currentDate[3]-48);  // ones
      if (currentDate[1] == '1') i = i + 10; // special case for 10, 11, 12, 13...
      if (i > 0) strcat(buffer,numberNames[i]);  // add 1, 2, ... 9, 10, ... 19
      if (currentDate[2] == '0' && i == 0 ) strcat(buffer,numberNames[0]); // ad

      strcat(buffer,".");
          
      display.setCursor(0, 88+charH-(charH*((int) strlen(buffer)/charW))); // figure out approx which line to start on based on character hight and width
    
      //split line to fit on screen
      const char * p = buffer;
      // keep going until we run out of text
      while (*p)
        {
        // find the position of the space
        const char * endOfLine = split (p, line_length); // split into line_length char lines
       
        // display up to that
        while (p != endOfLine)
          display.print (*p++);
         
        // finish that line
        display.println ();
       
        // if we hit a space, move on past it
        if (*p == ' ')
          p++;
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
  webpage("<p><font color=\"green\">Successfully updated SSID/password/timezone</font></p>");
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
      timeZone = charString;  // timeZone
      charString = "";
      phase++;
    }
    else if (phase == 3 && tmp == NULL)
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
  // fill remainder
  while (EEPROMAddr < EEPROM_SIZE)  // fill rest with nulls
  {
    EEPROM.write(EEPROMAddr, NULL);
    EEPROMAddr++;
  }
  EEPROM.commit();
}
