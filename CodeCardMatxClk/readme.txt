CodeCard Matrix Clock

This application is based on the Oracle CodeCard Hardware which is an ESP8266 base device with an ePaper display.

This displays the time as a matrix of letters that spell out the time in 5 minute increments.  The numbers in the bottom corner of the display are the additional minutes.

When turned on (via slide switch and then pressing Button A or B), the application will attempt to connect to the last known SSID and display the connected SSID and the device IP address.  If the SSID is not found, the application will create an Access Point titled 'CodeCard_AP' and show the device IP address.  Connect to the SSID and use the IP address to access the CodeCard configuration page to set the SSID and network password and any other application parameters.

The Clock application uses the worldtimeapi.org to synchronize the time and requires the desired timezone listed at http://worldtimeapi.org/timezones.  The clock is updated/synchronized twice a day at 12 AM and 2AM (one for day change one for DST check).  

The CodeCard buttons have no function in this application.


