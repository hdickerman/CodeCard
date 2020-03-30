void jacepro()
{
  strcpy(host,"joke.jace.pro");

  WiFiClientSecure client;
  client.setInsecure();
  
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  
  client.print(F("GET "));
  // Build URL
  client.print(F("/.netlify/functions/server"));
  // URL
  client.print(F(" HTTP/1.1\r\n"));
  client.print(F("Host: "));
    client.print(host);
    client.print(F("\r\n"));
  client.print(F("Accept: application/json\r\n"));
  client.print(F("User-Agent: CodeCard\r\n"));
  client.print(F("Connection: close\r\n\r\n"));
  Serial.println("request sent");

  if (client.connected()) 
  {
    for (int ii = 0; ii < sizeof(jokebuffer); ii++) {
      jokebuffer[ii] = 0; // Clears out old buffer
    }
    client.readBytesUntil('\r', jokebuffer, sizeof(jokebuffer));
    if (strcmp(jokebuffer, "HTTP/1.1 200 OK") != 0)
    {
      //Serial.println(status);
      Serial.println("Unexpected HTTP response");
    }
  
    //Skip Headers
    char endOfHeaders[] = "\r\n\r\n";
    client.find(endOfHeaders) || Serial.print("Invalid response");
  
    //Serial.println(client);  // show full response

    parse(0); // initialize parser
    jokebuffer[0]= 0;
    while (client.connected()) 
    {
    //String line = client.readStringUntil('\n');
      char c = client.read();
      if (c != 255)
      {
        parse(c);
        
        if (return_type == RETURN_DATA)
        {
          if (strcmp(key_value[1],"joke") == 0)      strcpy(jokebuffer, return_value);
          if (strcmp(key_value[1],"punchline") == 0)
          {
            strcat(jokebuffer," ");
            strcat(jokebuffer, return_value);
          }
        }
      }
    }
  
    client.stop();
    Serial.println(jokebuffer);
    
  }

return;

}
