// Adapted for CodeCard ePaper display from
// https://github.com/shadowedice/StockTicker
//
// Adapted from https://github.com/shadowedice/StockTicker
//
// Data provided by IEX Cloud Register for free API key at https://iexcloud.io 
// Free API Key limited to 50K messages/month or ~16 lookups every 15 min x 24hrs x 31 days
// Stocks are updated only when the US markets are open as returned by the API
// When the market is closed, isMarketOpen status is checked 4 times per hour x 24 hrs/day x 31 days/month = 2976 checks
// When the market is open, a new stock is refreshed and displayed 4 times per minute x 60 min/hrs x 7 hrs/day market is open x 5 days/week x 4 weeks/month = 33600 checks/month
// Stock displayed every 15 seconds
// Pressing Button A/1 redisplays previous stock, pressing Button B/2 displays next stock
// Buttons need to be pressed for 2 second to be recognized

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


String ssid = "";
String password = "";
String IPaddress = "";
String conn_ssid = "";

// Connection timeout;
#define CON_TIMEOUT   20*1000                     // milliseconds

#include <pgmspace.h>         // Using PROGMEM for Font
#include <GxEPD2_BW.h>        // Download/Clone and put in Arduino Library https://github.com/ZinggJM/GxEPD2
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

#include "arrows.h" // custom up/down arrows

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
int button1Cnt = 0;  // count of button press duration for debounce
int button2Cnt = 0;  // count of button press duration for debounce
bool btn1First = false;  //button 1 pressed before button 2
bool btn2First = false;  //button 2 pressed before button 1

#define CON_TIMEOUT   20*1000                     // milliseconds

bool connectSuccess = false;

// https://sandbox.iexapis.com/stable/stock/orcl/batch?types=quote&token=Tsk_0ad5f6c5027a443ab42b6dc829647c96  //Sandbox and Test token

const int MAX_NUM_TICKERS = 50;

int numTickers = 0;
String tickers[MAX_NUM_TICKERS];
char name[MAX_NUM_TICKERS][32] = {0}; // name of stock, max 32 chars
float lastp[MAX_NUM_TICKERS]; // current price
float changes[MAX_NUM_TICKERS]; // percent change
float opened[MAX_NUM_TICKERS];  // opening price
int refresh = 15; // interval in secs between lookups
unsigned long tickerStartTime = 0 ; // starting millis for delay between symbols
String apikey = "";  // IEXAPIS key from iexcloud.io
bool openCheck = false; // time to check if market is open
char isMarketOpen[] = "false"; // US market open status 'true' or 'false' (not boolean) and tickers should be updated
int currentTicker = MAX_NUM_TICKERS;  // current ticker pointer
char buffer[24] = {0}; // for formatting output line
int i = 0; // misc pointer/counter

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

const char* header = "<h1>CodeCard Stock Ticker Configuration</h1><p><form  name='wifi' method='post'>SSID:<input type='text' name='ssid'> Password:<input type='text' name='pass'><p>Timezone name:<input type='text' name='timezone'><p>IEXAPIS API key:<input type='text' name='apikey'><p><input type='submit' name ='config' value='Configure'></form><p>";
const char* tail = "<form  name='tickers' method='post'>Ticker:<input type='text' name='ticker'><input type='submit' name ='Add' value='Add'><input type='submit' name='Remove' value='Remove'></form>";

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
  Serial.print("Number of Tickers ");
  Serial.println(numTickers);

  //Connect to the wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.persistent(false); // only update flash with SSID/Pass if changed
  WiFi.enableInsecureWEP(); // Allow insecure encryptions
  WiFi.mode(WIFI_STA);
  WiFi.hostname("CCStock");  // Sets IP host name of device
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while ( WiFi.status() != WL_CONNECTED && millis() - start  < CON_TIMEOUT ) // couldn't connect, exceeded timeout
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  if ( WiFi.status() == WL_CONNECTED)  // successful?
  {
    IPaddress = WiFi.localIP().toString();  // Get connected IP
    connectSuccess = true;
    conn_ssid = "SSID: " + ssid; // connected to SSID
    Serial.println("WiFi connected");
  }
  else
  {
    connectSuccess = false;
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

       if ( mnt % 15 == 0 ) openCheck = true; // flag check to see if market is open every 15 mins

     } // minute and connected
   }  // done with minute stuff

    // This happens once every second

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
      //Nothing to do
    }
    else if (button1Cnt > 1 && button2Cnt == 0 ) // Only A button pressed
    {
      button1Cnt = 1; // partially clear counter
      // Do stuff for only button 1
      currentTicker = currentTicker - 2 ; //set ticker counter back 2, as it will increment
      if (currentTicker < 0) currentTicker = currentTicker + numTickers;  //wrap
      firstDisplay = true; // force screen refresh
    }
    else if (button1Cnt == 0 && button2Cnt > 1 ) // Only B button pressed
    {
      button2Cnt = 1; // partially clear counter
      // Do stuff for only button 2
      //no change to stock counter as will increment
      firstDisplay = true; // force screen refresh
    }
    else // none of those (no button pressed)
    {
      // nothing to do if nothing pressed
    }
   }  // end of button logic

   // see if 'refresh' seconds passed???  Yes, do stock stuff
   if (lastSec % 15 == 0 || firstDisplay == true ) // stock updates every 15 seconds or first time through
   {

      firstDisplay = false;
      
      if (connectSuccess == true && numTickers > 0) // if connected to a valid SSID, get and display stock quotes
      {
        currentTicker++;    // do next stock...
        if (currentTicker >= numTickers) currentTicker = 0;  // rollover pointer
        
        Serial.print("Ticker ");
        Serial.println(currentTicker);
    
        if (strcmp(isMarketOpen,"true") == 0 || strlen(name[currentTicker]) == 0  || openCheck == true ) // only update when market is open or not initialized, or market open check is needed
        {
            if (openCheck == true ) Serial.print("Mkt Open Check ");
            else                    Serial.print("Updating data ");
            Serial.println(tickers[currentTicker]);
            updateCurrentTicker();
            openCheck = false;  // only do an open check every 15 minutes ( isMarketOpen is set every stock update )
        }
     
        Serial.print(tickers[currentTicker]);
        Serial.print(" (");
        Serial.print(name[currentTicker]);
        Serial.print(") $");
        Serial.print(opened[currentTicker]);
        Serial.print(" $");
        Serial.print(lastp[currentTicker]);
        Serial.print(" ");
        Serial.print(changes[currentTicker]);
        Serial.print("% ");
        Serial.print("Mkt open: ");
        Serial.println(isMarketOpen);
    }

    displayCurrentTicker();

  }
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

void updateCurrentTicker() 
{

  // {"quote":{"symbol":"ORCL","companyName":"Oracle Corp.","primaryExchange":"c Nreea kogho tnEckwYxS","calculationPrice":"close","open":56.98,"openTime":1638487152735,"close":55.75,"closeTime":1632309125549,"high":57.17,"low":54.74,"latestPrice":55.25,"latestSource":"Close","latestTime":"December 6, 2019","latestUpdate":1582214920318,"latestVolume":9152646,"iexRealtimePrice":55.21,"iexRealtimeSize":104,"iexLastUpdated":1604881728977,"delayedPrice":55.82,"delayedPriceTime":1636441439105,"extendedPrice":56.27,"extendedChange":0,"extendedChangePercent":0,"extendedPriceTime":1600603768722,"previousClose":57.33,"previousVolume":10685677,"change":0.16,"changePercent":0.00276,"volume":9057950,"iexMarketPercent":0.03439395987658375,"iexVolume":310405,"avgTotalVolume":8426408,"iexBidPrice":0,"iexBidSize":0,"iexAskPrice":0,"iexAskSize":0,"marketCap":185774857004,"peRatio":18.19,"week52High":61.9,"week52Low":43.9,"ytdChange":0.217779,"lastTradeTime":1635475102798,"isUSMarketOpen":false}}

  strcpy(isMarketOpen,"false");  //error makes market closed

  WiFiClientSecure client;
  client.setInsecure();

  strcpy(host,"cloud.iexapis.com");

  //Serial.printf("\n[Connecting to %s ... ", host);
  if (client.connect(host, 443))
  {
    //Serial.println("connected]");

    //Serial.println("[Sending a request]");
    client.print("GET /stable/stock/");
    client.print(tickers[currentTicker]);
    client.print("/batch?types=quote&token=");
    client.print(apikey);
    client.print(" HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(host);
    client.print("\r\n");
    client.print("Connection: close\r\n\r\n");

    //Serial.println("[Response:]");
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
  //Serial.println(client);  // show full response
  
  memset (status, 0, sizeof(status)); // Clears out old buffer
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") == 0)  // header ok..
  {
    //Skip Headers
    char endOfHeaders[] = "\r\n\r\n";
    client.find(endOfHeaders) || Serial.print("Invalid response");
  
   //   Serial.print(">");
   //   Serial.print(client.readStringUntil('\n'));
   //   Serial.println("<");
  
    memset (status, 0, sizeof(status)); // Clears out old buffer}
    client.readBytesUntil(':', status, sizeof(status));  // See if valid response by reading up to first :
    //Serial.print("status ");
    //Serial.println(status);
    if (strcmp(status , "{\"quote\"") == 0) // valid results?
    {
      client.find("\"companyName\":\"");
      memset (name[currentTicker], 0, sizeof(name[currentTicker])); // Clears out old buffer
  
      client.readBytesUntil('\"', name[currentTicker], sizeof(name[currentTicker]));  // read to quote
  
      client.find("\"latestPrice\":");
      lastp[currentTicker] = client.parseFloat();
      
      client.find("\"previousClose\":");
      opened[currentTicker] = client.parseFloat();

      changes[currentTicker] = (lastp[currentTicker]- opened[currentTicker]) / opened[currentTicker] * 100 ;  // change from open (not % last change)
  
      memset (isMarketOpen, 0, sizeof(isMarketOpen)); // Clears out old buffer
      client.find("\"isUSMarketOpen\":") ;  // "isUSMarketOpen":  ,"isUSMarketOpen":false}
      client.readBytesUntil('}', isMarketOpen, sizeof(isMarketOpen));  // read to }
  
    }
  }
  else
  {
    Serial.print("Bad symbol ");
    Serial.println(tickers[currentTicker]);
    strcpy(name[currentTicker],"? Symbol not found ?");  // set name to not found
    lastp[currentTicker] = 0;
    changes[currentTicker] = 0;
    opened[currentTicker] = 0;
    strcpy(isMarketOpen,"false");  //error makes market closed
  }
  client.stop();
}

void displayCurrentTicker()
{
    Serial.print(hrs);
    Serial.print(":");
    Serial.print(mnt);
    Serial.print(":");
    Serial.print(sec);
    Serial.println("");
    
    display.setFullWindow();
    display.firstPage();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);

    // place time/date on screen
    display.setCursor(0, 15);
    //if ((hrs % 12) < 10) display.print(" ");
    display.print(hrs);
    display.print(":");
    if (mnt < 10) display.print("0");
    display.print(mnt);

    display.setCursor(154,15 );
    displayPrintPart(currentDate,5,8);  // Date in right
    display.print("-");  
    displayPrintPart(currentDate,0,4); 

    display.setFont(&FreeMonoBold18pt7b);  // Large font for symbol
    //Display Ticker
    display.getTextBounds(tickers[currentTicker], 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = 50;
    display.setCursor(x, y);
    display.print(tickers[currentTicker]);

    display.setFont(&FreeMono9pt7b);  // change to smaller font

    //Display company name
    memset(name[currentTicker]+24,0,1); // font only 24 char wide max, force terminator @ position 23
    display.getTextBounds(name[currentTicker], 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = y + 24;
    display.setCursor(x, y);
    display.print(name[currentTicker]); 

    //Display Open price
    strcpy(buffer, "Open:    ");
    dtostrf(opened[currentTicker], 8, 2, status); // convert float to char array reusing 'status', max value = 9999.99 with 1 space in front
    status[0] = '$';  // put a $ sign in
    strcat(buffer, status); // concatenate
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = y + 24;
    display.setCursor(x, y);
    display.print(buffer);

    display.setFont(&FreeMonoBold9pt7b);  // change to bold font

    //Display Currrent price
    strcpy(buffer, "Current: ");
    dtostrf(lastp[currentTicker], 8, 2, status); // convert float to char array reusing 'status', max value = 9999.99 with 1 space in front
    status[0] = '$'; // put a $ sign in
    strcat(buffer, status); // concatenate
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = y + 24;
    display.setCursor(x, y);
    display.print(buffer);

    if (changes[currentTicker] < 0) {display.drawBitmap(264 - x + 5, y - 16 , Arrow[ 0 ], 18 , 19 , GxEPD_BLACK );} //display down arrow
    if (changes[currentTicker] > 0) {display.drawBitmap(264 - x + 5, y - 16 , Arrow[ 1 ], 18 , 19 , GxEPD_BLACK );} //display up arrow

    display.setFont(&FreeMono9pt7b);  // change to regular font

    //Display Change %
    strcpy(buffer, "Change:  ");
    dtostrf(changes[currentTicker], 7, 2, status); // convert float to char array reusing 'status', max value = -999.99 with NO space in front
    strcat(status, "%");  // put a % at end
    strcat(buffer, status); // concatenate
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = y + 24;
    display.setCursor(x, y);
    display.print(buffer);

    // Display market open/closed status
    if (strcmp(isMarketOpen,"true") == 0)
    {
      strcpy(buffer, "US Market is open");
      display.setFont(&FreeMono9pt7b); // non-bolded
    }
    else 
    {
      strcpy(buffer, "US Market is closed");
      display.setFont(&FreeMonoBold9pt7b); // bolded
    }
    display.getTextBounds(buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    x = ((display.width() - tbw) / 2) - tbx;
    y = y + 24;
    display.setCursor(x, y);
    display.print(buffer);
    
    while (display.nextPage());
}

//just print char for char from a start position
void displayPrintPart(char* txt, byte start, byte len)
{
  for(byte i = 0; i < len; i++)
  {
    display.print(*(txt + start + i));
  }
  return;
}

// Config Webpage stuff...

String listOfTickers() 
{
  String html = "<ul style=\"list-style-type:none\">";
  for (int i = 0; i < numTickers; i++)
  {
    html += "<li>" + tickers[i] + "</li>";
  }

  html += "</ul>";
  return html;
}

void defaultPage() 
{
  webpage("");
}

void webpage(String status) 
{
  server.send(200, "text/html", header + status + listOfTickers() + tail);
}

void response() 
{
  if (server.hasArg("config"))  // config button pressed
  {
    if ((server.arg("ssid").length() > 0) && (server.arg("pass").length() > 0) )
    {
      ssid = server.arg("ssid");
      password = server.arg("pass");
    }
    if (server.arg("timezone").length() > 0)
    {
      timeZone = server.arg("timezone");
    }
    if (server.arg("apikey").length() > 0)
    {
      apikey = server.arg("apikey");
    }
    writeEEPROM();
    webpage("<p><font color=\"green\">Successfully updated configuration</font></p>");
  }
  else if (server.hasArg("Add") && (server.arg("ticker").length() > 0)) 
  {
    if (numTickers != MAX_NUM_TICKERS)
    {
      tickers[numTickers] =  server.arg("ticker");
      i= 0;
      while (tickers[numTickers][i])  // make upper case
        {
          tickers[numTickers][i] = toupper(tickers[numTickers][i]); // make uppercase
          i++;
        }
      numTickers++;
      writeEEPROM();
      webpage("<p><font color=\"green\">Successfully added " + server.arg("ticker") + "</font></p>");
    }
    else
    {
      webpage("<p><font color=\"red\">At maximum number of tickers!</font></p>");
    }
  }
  else if (server.hasArg("Remove") && (server.arg("ticker").length() > 0)) 
  {
    String tick = server.arg("ticker");
    tick.toUpperCase();  //  make uppercase
    bool found = false;
    for (int i = 0; i < numTickers; i++)
    {
      if (tickers[i] == tick)
        found = true;
      if (found && (i != (numTickers - 1)))
      {
        tickers[i] = tickers[i + 1]; // move up tickers
        strcpy(name[i], name[i + 1]); // move up names
        lastp[i] = lastp[i + 1]; // current price
        changes[i] = changes[i + 1]; // percent change
        opened[i] = opened[i + 1]; // open price
      }
    }
    if (found)
    {
      numTickers--;
      tickers[numTickers] = "";
      memset (name[numTickers], 0, sizeof(name[numTickers])); // null out name
      writeEEPROM();
      webpage("<p><font color=\"green\">Successfully removed " + tick + "</font></p>");
    }
    else
    {
      webpage("<p><font color=\"red\">Could not find " + tick + "</font></p>");
    }
  }
}

void readEEPROM()
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
      timeZone = charString;  // set timezone
      charString = "";
      phase++;
    }
    else if (phase == 3 && tmp == NULL)
    {
      apikey = charString;  // set apikey
      charString = "";
      phase++;
    }
    else if (phase == 4 && tmp == ';')
    {
      tickers[numTickers] = charString;
      numTickers++;
      charString = "";
    }
    else if (phase == 4 && tmp == NULL)
    {
      break;
    }
    else
      charString += tmp;
  }
}

void writeEEPROM()
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
  EEPROM.write(EEPROMAddr, NULL); // terminator
  EEPROMAddr++;
  //Write out apikey
  for (int i = 0; i < String(apikey).length(); i++)
  {
    EEPROM.write(EEPROMAddr, String(apikey).charAt(i));
    EEPROMAddr++;
  }
  EEPROM.write(EEPROMAddr, NULL); // terminator
  EEPROMAddr++;  
  //Write out tickers
  for (int i = 0; i < numTickers; i++)
  {
    for (int j = 0; j < tickers[i].length(); j++)
    {
      EEPROM.write(EEPROMAddr, toupper(tickers[i].charAt(j)));
      EEPROMAddr++;
    }
    EEPROM.write(EEPROMAddr, ';');
    EEPROMAddr++;
  }
  while (EEPROMAddr < EEPROM_SIZE)  // fill rest with nulls
  {
    EEPROM.write(EEPROMAddr, NULL);
    EEPROMAddr++;
  }
  EEPROM.commit();
}
