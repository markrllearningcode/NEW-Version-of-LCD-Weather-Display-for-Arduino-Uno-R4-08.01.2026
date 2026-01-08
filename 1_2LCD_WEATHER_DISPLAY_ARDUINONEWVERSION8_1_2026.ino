#include "arduino_secrets.h"
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

LiquidCrystal_I2C lcd(0x27, 16, 2);

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int status = WL_IDLE_STATUS;
char server[] = "api.openweathermap.org";
WiFiClient client;

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 60UL * 1000UL;

unsigned long lastPageSwitchTime = 0;
const unsigned long pageInterval = 4000UL;

enum Page {
  PAGE_TIME_LOCATION = 0,
  PAGE_WEATHER,
  PAGE_TEMP,
  PAGE_HUMIDITY,
  PAGE_PRESSURE,
  PAGE_WIND,
  PAGE_COUNT
};

Page currentPage = PAGE_TIME_LOCATION;

bool haveWeather = false;
String cachedWeather = "--";
float cachedTempC = NAN;
int cachedHumidity = -1;
float cachedPressure_hPa = NAN;
float cachedWindMS = NAN;

void printWifiStatus();
void httpRequest();
void read_response();
void drawPage();


void setup() {

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  if(WiFi.status() == WL_NO_MODULE) {
    Serial.println("Commmunication with WiFi module failed!");
    lcd.clear();
    lcd.print("WiFi module err");
    while(true) {}
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  while(status != WL_CONNECTED) {
    Serial.print("Attempting to conect to SSID: ");
    Serial.print(ssid);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi connecting");

    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  printWifiStatus();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");

  timeClient.begin();
  timeClient.setTimeOffset(3600 * 1);

  httpRequest();

  drawPage();
}

void loop() {

  read_response();
  timeClient.update();

  if (!lastConnectionTime || millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  }

  if (millis() - lastPageSwitchTime > pageInterval) {
    lastPageSwitchTime = millis();
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    drawPage();
  }
}

void read_response() {
  String payload = "";
  bool jsonDetected = false;

  while(client.available()) {
    char c = client.read();

    Serial.print(c);

    if (c == '{') jsonDetected = true;
    if (jsonDetected) payload += c;
  }

  if (!jsonDetected) return;

  DynamicJsonDocument root(1024);
  DeserializationError error = deserializeJson(root, payload);
  if (error) {
    Serial.print("Deserialization failed:");
    Serial.println(error.c_str());
    return;
  }

  cachedWeather = String((const char*)root["weather"][0]["main"]);
  cachedTempC = (float)root["main"]["temp"] - 273.15;
  cachedHumidity = (int)root["main"]["humidity"];
  cachedPressure_hPa = (float)root["main"]["pressure"];
  cachedWindMS = (float)root["wind"]["speed"];

  haveWeather = true;

  drawPage();
}

void httpRequest() {
  client.stop();

  String req = String("GET /data/2.5/weather?q=") + LOCATION + "&appid=" + API_KEY + " HTTP/1.1";

  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println(req);
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();

    lastConnectionTime = millis();
  } else {
    Serial.println("connection failed");
  }
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println("dBm");
}

void drawPage() {
  lcd.clear();

  const char* days[7] = {"Sunday ", "Monday ", "Tuesday ", "Wednesday ", "Thursday ", "Friday ", "Saturday "};

  int hh = timeClient.getHours();
  int mm = timeClient.getMinutes();

  if(!haveWeather && currentPage != PAGE_TIME_LOCATION){
    lcd.setCursor(0, 0);
    lcd.print("Fetching weather");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    return;
  }

  switch (currentPage) {

    case PAGE_TIME_LOCATION: {

      lcd.setCursor(0, 0);
      lcd.print(days[timeClient.getDay()]);
      lcd.print("");
      if(hh < 10) lcd.print("0");
      lcd.print(hh);
      lcd.print(":");
      if(mm < 10) lcd.print("0");
      lcd.print(mm);

      lcd.setCursor(0, 1);
      lcd.print(LOCATION);
      break;
    }

    case PAGE_WEATHER: {
      lcd.setCursor(0, 0);
      lcd.print("Weather:");
      lcd.setCursor(0, 1);
      lcd.print(cachedWeather);
      break;
    }

    case PAGE_TEMP: {
      lcd.setCursor(0, 0);
      lcd.print("Temperature");
      lcd.setCursor(0, 1);
      lcd.print(cachedTempC, 1);
      lcd.print((char)223);
      lcd.print("C");
      break;
    }

    case PAGE_HUMIDITY: {
      lcd.setCursor(0, 0);
      lcd.print("Humidity:");
      lcd.setCursor(0, 1);
      lcd.print(cachedHumidity);
      lcd.print("%");
      break;
    }

    case PAGE_PRESSURE: {
      lcd.setCursor(0, 0);
      lcd.print("Pressure:");
      lcd.setCursor(0, 1);
      lcd.print(cachedPressure_hPa, 0);
      lcd.print(" hPa");
      break;
    }

    case PAGE_WIND: {
      lcd.setCursor(0, 0);
      lcd.print("Wind Speed:");
      lcd.setCursor(0, 1);
      lcd.print(cachedWindMS, 1);
      lcd.print(" m/s");
      break;
    }

    default:
    break;    
  }
}
