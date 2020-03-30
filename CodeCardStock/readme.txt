CodeCard Stock Ticker

This application is based on the Oracle CodeCard Hardware which is an ESP8266 base device with an ePaper display.

When turned on (via slide switch and then pressing Button A or B), the application will attempt to connect to the last known SSID and display the connected SSID and the device IP address.  If the SSID is not found, the application will create an Access Point titled 'CodeCard_AP' and show the device IP address.  Connect to the SSID and use the IP address to access the CodeCard configuration page to set the SSID and network password and any other application parameters.

The Stock Ticker application uses the worldtimeapi.org to synchronize the time and requires the desired timezone listed at http://worldtimeapi.org/timezones.  The clock is updated/synchronized twice a day at 12 AM and 2AM (one for day change one for DST check).  

Stock data is provided by IEX Cloud.  You must register for free API key at https://iexcloud.io and specify it on the configuration page.  Note that the free API Key limited to 50K messages/month.  As such stocks are updated only when the US markets are open as returned by the API.  When the market is closed, the Market Open status is checked 4 times per hour x 24 hrs/day x 31 days/month = 2976 checks.  When the market is open, each stock is display and refreshed 4 times per minute x 60 min/hrs x 7 hrs/day market is open x 5 days/week x 4 weeks/month = 33600 checks.   This keeps the total number of API calls under the 50K/month limit.

The list of stocks displayed are entered via the configuration screen.

Stocks are displayed every 15 seconds.
Pressing Button A/Button 1 redisplays previous stock, pressing Button B/Button 2 displays next stock.
The Buttons need to be pressed for 2 second to be recognized.


