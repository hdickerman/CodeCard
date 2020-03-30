// Adapted for CodeCard ePaper display from
// https://github.com/shadowedice/StockStory
//
// Adapted from https://github.com/shadowedice/StockStory
//
// Data provided by newsapi.org for free API key at http//newsapi.org
// Free API Key limited to 500 requests/day or about request every 3 min (24hrs * 60 minutes / 500)
// News stories displayed every 20 seconds
// Pressing Button A/1 redisplays previous news story, pressing Button B/2 displays next story
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

#include "SAXJsonParser.h" // Custom JSON parser

#include <WiFiClient.h>
char host[100] ;  // character array for hostname
const int httpPort = 80;


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
#include "Monospaced_plain_10.h"

#define DISPLAY_WIDTH 36 // width of display in characters

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

//News buffer and sorting
#define MAX_STORY_LEN  500 // maximum length of text (title + description)
#define MAX_KEY_LEN 20  // maximum length of key  (publishedAt)
#define MAX_ENTRIES 20 // maximum number of entries
int ii = 0;
int jj = 0;
byte ptr[MAX_ENTRIES] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}; // pointer to elements in character key array, initialized (<256)
byte ptrEntries = 0; // number of entries in pointers
byte ptrSaved = 0; // saved pointer
char keyBuffer[MAX_ENTRIES][MAX_KEY_LEN]  = {32}; // fill with spaces
char storyBuffer[MAX_ENTRIES][MAX_STORY_LEN] = {0};
char newKey[MAX_KEY_LEN] = {0};
char newsStory[MAX_STORY_LEN] = {0};
bool storyAdded = false;
int currentStory = 0; // current story to display

int refresh = 15; // interval in secs between display

String apikey = "";  // NEWSAPI key from newsapi.org
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

const char* header = "<h1>CodeCard News Story Configuration</h1><p><form  name='wifi' method='post'>SSID:<input type='text' name='ssid'> Password:<input type='text' name='pass'><p>Timezone name:<input type='text' name='timezone'><p>NewsAPI.org API key:<input type='text' name='apikey'><p><input type='submit' name ='config' value='Configure'></form><p>";
const char* tail = "";

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

//substitute or delete search char from buffer with replacement (0=remove) MUST BE ZERO/NULL terminated!
void substituteChar(char buffer[], char seachChar, char replacement)
{

  int i = 0;
  int ii = 0;
  while ( buffer[ i + ii ] != 0) // loop till null terminator
  {
    if (buffer[i] == (byte) seachChar)
    {
      if (replacement > 0) buffer[i] = replacement;  // do substitution
      else
      {
        ii++ ;// point to next character to remove extra char and shift char array down
        i-- ;//  index will to negate increment
      }
    }
    i++;
    if (ii > 0) buffer[i] = buffer[i + ii]; // shifts down if ii > i
  }
  return;
}

void setup()
{
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH); //immediately set wake pin to HIGH to keep the chip enabled
  
  //Set buttons
  pinMode(BUTTON1_PIN, INPUT_PULLUP);  // Button 1 = A
  pinMode(BUTTON2_PIN, INPUT_PULLUP);  // Button 2 = B
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

  //Read EEPROM to get ssid/password and stock Storys
  readEEPROM();

  //Connect to the wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.persistent(false); // only update flash with SSID/Pass if changed
  WiFi.enableInsecureWEP(); // Allow insecure encryptions
  WiFi.mode(WIFI_STA);
  WiFi.hostname("CCNews");  // Sets IP host name of device
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
  //  timer1_write(num_ticks); // num_ticks * ticks/us sec = time (us)

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
          Serial.println((float)(timeCur - timeCurLast) / secondsCountLast);
        }
      }

      if ( mnt % 5 == 0 || firstDisplay == true) // get new news stories every 5 min or at start
      {
        getNewsStories();
        if (storyAdded == true) currentStory = 0; // if new story, reset currentStory to beginning
      }

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
      currentStory = currentStory - 2 ; //set story counter back 2, as it will increment
      if (currentStory < 0) currentStory = currentStory + ptrEntries;  //wrap
      firstDisplay = true; // force screen refresh
    }
    else if (button1Cnt == 0 && button2Cnt > 1 ) // Only B button pressed
    {
      button2Cnt = 1; // partially clear counter
      // Do stuff for only button 2
      //no change to story counter as will increment
      firstDisplay = true; // force screen refresh
    }
    else // none of those (no button pressed)
    {
      // nothing to do if nothing pressed
    }
  }  // end of button logic

  // see if 'refresh' seconds passed???  Yes, do news display
  if (lastSec % 20 == 0 || firstDisplay == true ) // story updates every 20 seconds or first time through
  {

    firstDisplay = false;

    if (connectSuccess == true) /// if connected to a valid SSID, get and display news stories
    {
      currentStory++;    // do next story...
      if (currentStory >= ptrEntries) currentStory = 0;  // rollover pointer based on number of pointer entries

      displayNewsStory();
    }
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
  strcpy(host, "worldtimeapi.org");

  WiFiClient client;

  //Serial.Serial.print("\n[Connecting to %s ... ", host);
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

void insertNewsStory()  // See if news story is new or already stored based on publishedAt time, add if new
{
  ii = 0;
  storyAdded = false;  // if new story was added

  while  (ii <  MAX_ENTRIES) // try all current entries
  {
    if (strcmp(newKey, keyBuffer[ptr[ii]]) > 0) // newKey > keyBuffer[], make room
    {
      /// Uncomment for debugging
      Serial.print("Inserting at ");
      Serial.print(ii);
      Serial.print(" ");
      Serial.print(newKey);
      Serial.print(" ");
      Serial.print(keyBuffer[ptr[ii]]);
      Serial.print(" ");
      Serial.print("PtrEntries ");
      Serial.print(ptrEntries);
      Serial.print(" ");
      Serial.print("Place data at ");
      Serial.print(ptr[ptrEntries]);
      Serial.print(" Dropping ");
      Serial.print(keyBuffer[ptr[ptrEntries]]);
      Serial.println();
      *// ///

      strcpy(keyBuffer[ptr[ptrEntries]], newKey); // insert new value in place of one getting dropped...(key)
      strcpy(storyBuffer[ptr[ptrEntries]], newsStory); // insert new value in place of one getting dropped... (story)

      ptrSaved =  ptr[ptrEntries]; // point inserted value next position (before shift)

      for (jj =  ptrEntries; jj > ii ; jj--) // shift stuff down to make room
      {
        ptr[jj] = ptr[jj - 1]; // shift down
      }

      ptr[ii] =  ptrSaved; // point inserted value to position at end of array (after shift)

      if (ptrEntries < MAX_ENTRIES - 1) ptrEntries++; //count number of entries, till max
      storyAdded = true;
      ii = MAX_ENTRIES; // force stop

      // dump for debugging
      /*/     /// Uncomment for debugging
        Serial.println("Pointers");
        for (jj = 0; jj< MAX_ENTRIES; jj++)
        {
          Serial.print(jj);
          Serial.print(" ");
          Serial.print(ptr[jj]);
          Serial.print(" ");
          Serial.print(keyBuffer[ptr[jj]]);
          Serial.print(" ");
          ///Serial.print(storyBuffer[ptr[jj]]);
          Serial.println();
        }

        Serial.println("Buffers");
        for (jj = 0; jj< MAX_ENTRIES; jj++)
        {
          Serial.print(jj);
          Serial.print(" ");
          Serial.print(keyBuffer[jj]);
          Serial.print(" ");
          //Serial.print(storyBuffer[jj]);
          Serial.println();
        }
      */ /// Uncomment for debugging

    }

    else if (strcmp (newKey, keyBuffer[ptr[ii]]) == 0) // newKey = keyBuffer[]
    {
      //Serial.print("Exists at ");
      //Serial.print(ii);
      //Serial.println();
      ii = MAX_ENTRIES; // force stop
    }

    ii++; // try next one
  }

  return ;
}

void getNewsStories()
{
  Serial.println("Updating News Stories");

  char c;
  parse(0);  // initialize parser

  WiFiClient client;

  strcpy(host, "newsapi.org");
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // newsapi.org/v2/top-headlines?apiKey=XXXXXXXXXXXXXXXXX&country=us
  client.print(F("GET "));
  // Build URL
  client.print(F("/v2/top-headlines?apiKey="));
  client.print(apikey);
  client.print(F("&country=us"));  //this can be replaced with list of sources "&sources=bbc-news,cnn"
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

  Serial.println("JSON request sent");
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
        if      (strcmp(key_value[4], "name") == 0)
        {
          //Serial.println(return_value) ; //
        }
        if      (strcmp(key_value[3], "title") == 0)
        {
          //Serial.println(return_value) ; // get title
          strcpy(newsStory, return_value);
        }
        if      (strcmp(key_value[3], "description") == 0)
        {
          if (strlen(return_value) > 20 && strlen(newsStory) + strlen(return_value) < sizeof(newsStory))
          {
            strcat(newsStory, ": ");
            strcat(newsStory, return_value);
          }
          //Serial.println(return_value) ; //
          substituteChar(return_value, '\n', 0); // remove new line chars
          substituteChar(return_value, '\r', 32); // replace returns with space
        }
        if      (strcmp(key_value[3], "publishedAt") == 0)       // 2020-03-14T02:38:46Z
        {
          //Serial.println(return_value) ; //
          return_value[10] = ' '; // change to space
          return_value[19] = 0; // truncate extra char
          strcpy(newKey, return_value);
          insertNewsStory();  // add if new news story
          //Serial.print("storyAdded ");  /// see if it should be added....
          //Serial.println(storyAdded);  /// see if it should be added....
          if (storyAdded)
          {
            //Serial.print("Story added: ");
            //Serial.println(newsStory);
          }
        }
      }
      // Decode JSON data work is all above
    }
  }
  Serial.print(charCount);
  Serial.println(" Characters read");
  Serial.println("All Done");

  // dump for debugging
  /*
    Serial.println("Pointers");
    for (jj = 0; jj< MAX_ENTRIES; jj++)
    {
      Serial.print(jj);
      Serial.print(" ");
      Serial.print(ptr[jj]);
      Serial.print(" ");
      Serial.print(keyBuffer[ptr[jj]]);
      Serial.println();
      Serial.print(storyBuffer[ptr[jj]]);
      Serial.println();
    }

    Serial.println("Buffers");
    for (jj = 0; jj< MAX_ENTRIES; jj++)
    {
      Serial.print(jj);
      Serial.print(keyBuffer[jj]);
      Serial.println();
      Serial.print(storyBuffer[jj]);
      Serial.println();
    }
    /**////
}

void displayNewsStory()
{
  //Show time for debugging
  Serial.print(hrs);
  Serial.print(":");
  Serial.print(mnt);
  Serial.print(":");
  Serial.print(sec);
  Serial.print("  Story:");
  Serial.print(currentStory);
  Serial.println("");

  display.init();
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.setTextWrap(true);
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);

  // place time/date on screen
  display.setFont(&FreeMonoBold9pt7b); // 18 px high 14 px wide
  display.setCursor(0, 16);
  //if ((hrs % 12) < 10) display.print(" ");
  display.print(hrs);
  display.print(":");
  if (mnt < 10) display.print("0");
  display.print(mnt);

  display.setCursor(154, 16 );
  displayPrintPart(currentDate, 5, 8); // Date in right
  display.print("-");
  displayPrintPart(currentDate, 0, 4);

  display.setFont(&Monospaced_plain_10);

  display.setCursor(0, 32); //line to start on

  //split line to fit on screen
  const char * p = storyBuffer[ptr[currentStory]];
  // keep going until we run out of text
  while (*p)
  {
    // find the position of the space
    const char * endOfLine = split (p, DISPLAY_WIDTH); // split into DISPLAY_WIDTH char lines

    // display up to that
    while (p != endOfLine)
      display.print (*p++);

    // finish that line
    display.println ();

    // if we hit a space, move on past it
    if (*p == ' ')
      p++;
  }

  // time of story
  display.print(currentStory);
  display.print(" (");
  display.print(keyBuffer[ptr[currentStory]]);  // date and time of story (publishedAt)
  display.print(")");
  display.println();

  while (display.nextPage());
}

//just print char for char from a start position
void displayPrintPart(char* txt, byte start, byte len)
{
  for (byte i = 0; i < len; i++)
  {
    display.print(*(txt + start + i));
  }
  return;
}

// Config Webpage stuff...

void defaultPage()
{
  webpage("");
}

void webpage(String status)
{
  server.send(200, "text/html", header + status + tail);
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
  while (EEPROMAddr < EEPROM_SIZE)  // fill rest with nulls
  {
    EEPROM.write(EEPROMAddr, NULL);
    EEPROMAddr++;
  }
  EEPROM.commit();
}
