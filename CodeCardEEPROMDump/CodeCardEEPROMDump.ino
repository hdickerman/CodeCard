// Dump EEPROM on CodeCard Display and Serial output

#include <EEPROM.h>


#include <pgmspace.h>         // Using PROGMEM for Font
#include <GxEPD2_BW.h>        // Download/Clone and put in Arduino Library https://github.com/ZinggJM/GxEPD2
#include <Fonts/FreeMonoBold9pt7b.h>

char buffer[24] = {0};
int readCount = 0;
const int EEPROM_SIZE = 512;  //Can be max of 4096


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
  display.setFullWindow();
  display.setTextWrap(true);
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 12);

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++)
  {

    char c = EEPROM.read(i);

    Serial.print("0x");
    Serial.print( c , HEX );
    Serial.print(", ");

    if (c < 32) c = 32;  // if non-printable, make a space
    
    buffer[(readCount % 10)] = c; // 
    
    display.print(c);  // place on display
    
    buffer[(readCount % 10) + 1 ] = 0; // null terminate
    readCount = readCount + 1;
    if (readCount % 10 == 0) 
    {
      Serial.print("   // ");
      Serial.println(buffer);
      buffer[0] = 0 ; // clear buffer
    }
  }

  while (display.nextPage());
    
  Serial.print("   // ");
  Serial.println(buffer);
  Serial.println();

  

  Serial.println("All done");

}


void loop() 
{
}
