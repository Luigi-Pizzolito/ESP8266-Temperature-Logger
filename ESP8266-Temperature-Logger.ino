/*
   Libraries needed
*/
//Normal Mode
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
//Config. Mode
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
//Temperature Sensing.
#include <OneWire.h>

/*
   Configurable Settings

   Connect CONFIG_PIN(Default GPIO_03[RX]) to GND during startup to enter Config. Mode:
      1. Connect to 'ESP_Logger' WiFi Access Point, with the password 'wifilogger'
      2. Visit http://192.168.4.1 to open the configuration page, or  http://192.168.4.1/edit to see the filesystem.
      3. After seeting your values, click on the 'Save' button then the 'Restart'
      4. Your logger is now configured.
*/
String ssid;
String password;
String host;
String url;
int wc_p;         // max. time in seconds to connect to wifi, before giving up
int gr_p;         // max. times of attemps to perform GET request, before giving up
bool s_vcc;       //wether to send VCC voltage as a parameter in the url request.
bool is_ip;       //wether host adress is IP
String vcc_parm;  //parameter to pass VCC voltage by.
String temp_parm; //parameter to pass temperature by.
long sleepTime;

//temp Sensing
float celsius;

/*
   System Variables
*/
#define NOT_DEBUG //wether to enable debug or to show indication lights instead
//Normal Mode
int failCount = 0;
ADC_MODE(ADC_VCC);
bool su_mode = true;
//Config. Mode
#define CONFIG_PIN 3
ESP8266WebServer server(80);
File fsUploadFile;
const char *APssid = "ESP_Logger";
const char *APpass = "wifilogger";
//Temp Sensing
OneWire ds(2); // on pin 2 (a ~4.7K resistor is necessary)

void setup()
{
  //start serial monitor, SPIFFS and Config. Pin
  pinMode(16, WAKEUP_PULLUP);
  WiFi.persistent(false);
  Serial.begin(115200);
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  delay(10);
  SPIFFS.begin();
  Serial.println();
  Serial.println("Logger Booting...");

  //read Config. Pin
  su_mode = !digitalRead(CONFIG_PIN);
  if (su_mode)
  {

//start Config. Mode
#ifdef NOT_DEBUG
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
#endif
    Serial.println("Entering Config. Mode!");

    //start WiFi Access Point
    Serial.println("Configuring Access Point...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(APssid, APpass);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP Address: ");
    Serial.println(myIP);

    //start HTTP server

    //edit pages
    server.on("/list", HTTP_GET, handleFileList);
    server.on("/edit", HTTP_GET, []() {
      if (!handleFileRead("/edit.htm"))
        server.send(404, "text/plain", "File Not Found");
    });
    server.on("/edit", HTTP_PUT, handleFileCreate);
    server.on("/edit", HTTP_DELETE, handleFileDelete);
    server.on("/edit", HTTP_POST, []() { server.send(200, "text/plain", ""); }, handleFileUpload);

    //pages from SPIFFS
    server.onNotFound([]() {
      if (!handleFileRead(server.uri()))
      {
        server.send(404, "text/plain", "File Not Found");
      }
    });

    //temp serving page
    server.on("/temp", HTTP_GET, []() { celsius = getTemp(); server.send(200, "text/plain", String(celsius)); });
    server.on("/temp/", HTTP_GET, []() { celsius = getTemp(); server.send(200, "text/plain", String(celsius)); });

    server.begin();
    Serial.println("HTTP Server Started");
  }
  else
  {
    //start Normal Mode
    Serial.println("SPIFFS Content: ");
    {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next())
      {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
      }
      Serial.printf("\n");
    }

    //read and parse config.json from SPIFFS
    readConfig();

    //connect to WiFi
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      //if we are taking too long to connect to WiFi give up.
      failCount++;
      if (failCount == wc_p * 2)
      {
        Serial.print("Session Terminated. Giving up After ");
        Serial.print(wc_p);
        Serial.println(" Tries Connecting to WiFi.");
        delay(20);
        fail();
        sleepNow();
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    failCount = 0;
  }
}

void loop()
{
  if (su_mode)
  {
    server.handleClient();
  }
  else
  {
    //if we have tried too many times to make a GET Request give up.
    ++failCount;
    if (failCount == gr_p + 1)
    {
      Serial.print("Session Terminated. Giving up After ");
      Serial.print(gr_p);
      Serial.println(" Tries Doing the GET Request.");
      delay(20);
      fail();
      sleepNow();
    }

    Serial.print("Try: ");
    Serial.println(failCount);
    Serial.print("Connecting to ");
    Serial.println(host);

    //try to connect to the host with TCP
    WiFiClient client;
    const int httpPort = 80;
    if (is_ip)
    {
      IPAddress addr;
      if (addr.fromString(host))
      {
        if (!client.connect(addr, httpPort))
        {
          //try again if the connection fails.
          Serial.println("Connection To IP Failed");
          delay(10);
          return;
        }
      }
      else
      {
        Serial.println("Failed to Convert IP String to IP Address.");
        while (1)
          ;
      }
    }
    else
    {
      if (!client.connect(host.c_str(), httpPort))
      {
        //try again if the connection fails.
        Serial.println("Connection Failed");
        delay(10);
        return;
      }
    }

    //create the URI for the request
    url += "?";

    if (s_vcc)
    {
      //read vcc
      url += vcc_parm;
      url += "=";
      uint32_t getVcc = ESP.getVcc();
      String VccVol = String((getVcc / 1000U) % 10) + "." + String((getVcc / 100U) % 10) + String((getVcc / 10U) % 10) + String((getVcc / 1U) % 10);
      url += VccVol;
      url += "&";
    }
    //read temperature
    url += temp_parm;
    url += "=";
    celsius = getTemp();
    url += String(celsius);

    //request url to server
    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host.c_str() + "\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0)
    {
      if (millis() - timeout > 60000)
      {
        //give up if the server takes too long to reply.
        Serial.println(">>> Client Timeout !");
        client.stop();
        //return;
        sleepNow();
      }
    }

    //print response to serial
    while (client.available())
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    //finish request and close connections
    client.stop();
    Serial.println();
    Serial.println("Closing Connection");

    //enter Deep Sleep
    // Serial.println("Entering Deep Sleep");
    delay(100);
    yay();
    sleepNow();
  }
}

/*
   Universal Functions
*/

//temp reading
float getTemp()
{
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  int failCountWire = 0;

  if (!ds.search(addr))
  {
    failCountWire++;
    if (failCountWire == 5)
    {
      Serial.print("Reding Terminated. Giving up After 5 Tries Reading Sensor...");
      delay(20);
      fail();
      sleepNow();
    }
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    // return;
  }

  Serial.print("ROM =");
  for (i = 0; i < 8; i++)
  {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7])
  {
    Serial.println("CRC is not valid!");
    delay(20);
    fail();
    sleepNow();
    // return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0])
  {
  case 0x10:
    Serial.println("  Chip = DS18S20"); // or old DS1820
    type_s = 1;
    break;
  case 0x28:
    Serial.println("  Chip = DS18B20");
    type_s = 0;
    break;
  case 0x22:
    Serial.println("  Chip = DS1822");
    type_s = 0;
    break;
  default:
    Serial.println("Device is not a DS18x20 family device.");
    delay(20);
    fail();
    sleepNow();
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for (i = 0; i < 9; i++)
  { // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  int16_t raw = (data[1] << 8) | data[0];
  if (type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  return celsius;
}

//enter deep Sleep
void sleepNow()
{
  yield();
  //delay(120000);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  // WiFi.forceSleepBegin();

  Serial.println("Entering Deep sleep...");
  delay(300);
  yield();
  ESP.deepSleep(sleepTime * 60 * 1000000, WAKE_RF_DEFAULT);
  delay(1000);
}

void fail()
{
  //something has gone wrong, blink an indicator on the LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
}

void yay()
{
  //everything worked, blink an indicator on the LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(350);
  digitalWrite(LED_BUILTIN, HIGH);
}

void readConfig()
{
  //read config.json and load configuration to variables.
  File configFile = SPIFFS.open("/config.jsn", "r");
  if (!configFile)
  {
    Serial.println("Failed to Open Config. File");
    delay(20);
    fail();
    sleepNow();
  }
  size_t size = configFile.size();
  if (size > 1024)
  {
    Serial.println("Config. File Size is too Large");
    delay(20);
    fail();
    sleepNow();
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(buf.get());
  if (!json.success())
  {
    Serial.println("Failed to Parse config. File");
    delay(20);
    fail();
    sleepNow();
  }

  ssid = (const char *)json["ssid"];
  password = (const char *)json["pass"];
  host = (const char *)json["host"];
  url = (const char *)json["uri"];
  wc_p = json["wc_p"];
  gr_p = json["gr_p"];
  s_vcc = json["s_vcc"];
  is_ip = json["is_ip"];
  vcc_parm = (const char *)json["vcc_p"];
  temp_parm = (const char *)json["temp_p"];
  sleepTime = json["sleep"];

  Serial.println("Parsed JSON Config.");
  Serial.print("Loaded ssid: ");
  Serial.println(ssid);
  Serial.print("Loaded password: ");
  Serial.println(password);
  Serial.print("Loaded host: ");
  Serial.println(host);
  Serial.print("Loaded IsIP: ");
  Serial.println(is_ip);
  Serial.print("Loaded uri: ");
  Serial.println(url);
  Serial.print("Loaded WiFi Connect Persistance: ");
  Serial.println(wc_p);
  Serial.print("Loaded GET Request Persistance: ");
  Serial.println(gr_p);
  Serial.print("Loaded Send VCC: ");
  Serial.println(s_vcc);
  Serial.print("Loaded VCC Param.: ");
  Serial.println(vcc_parm);
  Serial.print("Loaded Temp. Param.: ");
  Serial.println(temp_parm);
  Serial.print("Loaded Sleep Time: ");
  Serial.println(sleepTime);
  Serial.println();
}

/*
   Config. Mode Functions
*/

//edit functions
String formatBytes(size_t bytes)
{
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  else if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  else
  {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename)
{
  if (server.hasArg("download"))
    return "application/octet-stream";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if ((path == "/") && (server.argName(0) == "restart" && server.arg(0) == "true"))
  {
    Serial.println("requested reset from admin page!");
    server.send(200, "text/plain", "Restarting!");
#ifdef NOT_DEBUG
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    delay(200);
    wdt_reset();
    ESP.restart();
    while (1)
    {
      wdt_reset();
    }
    //WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while (1)wdt_reset();
  }
  if (path.endsWith("/"))
    path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload()
{
  if (server.uri() != "/edit")
    return;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    Serial.print("handleFileUpload Name: ");
    Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: ");
    Serial.println(upload.totalSize);
  }
}

void handleFileDelete()
{
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate()
{
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList()
{
  if (!server.hasArg("dir"))
  {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next())
  {
    File entry = dir.openFile("r");
    if (output != "[")
      output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}
