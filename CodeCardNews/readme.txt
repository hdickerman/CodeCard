CodeCard News

This application is based on the Oracle CodeCard Hardware which is an ESP8266 base device with an ePaper display.

When turned on (via slide switch and then pressing Button A or B), the application will attempt to connect to the last known SSID and display the connected SSID and the device IP address.  If the SSID is not found, the application will create an Access Point titled 'CodeCard_AP' and show the device IP address.  Connect to the SSID and use the IP address to access the CodeCard configuration page to set the SSID and network password and any other application parameters.

The News application uses the worldtimeapi.org to synchronize the time and requires the desired timezone listed at http://worldtimeapi.org/timezones.  The clock is updated/synchronized twice a day at 12 AM and 2AM (one for day change one for DST check).  

News data is provided by newsapi.org.  You must register for free API key at http//newsapi.org.  Free API Key limited to 500 requests/day or about a request every 3 min (24hrs * 60 minutes / 500).  News stories are loaded in newest to oldest and will hold the last 20 stories.  As newer news stories are loaded, the older stories are dropped.  News stories are displayed every 20 seconds on a rotating basis.

Pressing Button A/Button 1 redisplays previous news story, pressing Button B/Button 2 displays next news story.
The Buttons need to be pressed for 2 second to be recognized.

Application uses custom SAXJsonParser that only loads on branch of a JSON at a time, sequentially to save memory.


