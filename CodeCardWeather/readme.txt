CodeCard Weather Forecast

This application is based on the Oracle CodeCard Hardware which is an ESP8266 base device with an ePaper display.

When turned on (via slide switch and then pressing Button A or B), the application will attempt to connect to the last known SSID and display the connected SSID and the device IP address.  If the SSID is not found, the application will create an Access Point titled 'CodeCard_AP' and show the device IP address.  Connect to the SSID and use the IP address to access the CodeCard configuration page to set the SSID and network password and any other application parameters.

The Weather Forecast application uses the Darksky Weather API.  You must register for free API key at https://darksky.net/dev.  The weather forecast is provided based on the latitude and longitude.of the desired location.  A location's latitude and longitude can be determied via the use of maps.google.com.  Use Google Maps to map the desired location.  In the resulting URL the 2 numbers after the @ sign are the location's latitude and longitude.  Boston MA is at 42.3550483,-71.0656512.  The Darksky API key and latitude and longitude are entered on the configuration page.  Time is derived from the DarkSky API

The CodeCard buttons have no function in this application.

Application uses custom SAXJsonParser that only loads on branch of a JSON at a time, sequentially to save memory.

