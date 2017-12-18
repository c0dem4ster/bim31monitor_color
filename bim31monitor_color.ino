#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>

#include "fonts.h"
#include "bmps.h"

#define D_BLACK 0
#define D_DARKGREY 1
#define D_RED 2
#define D_LIGHTGREY 3

#define TFT_DC D2
#define TFT_CS D1
#define TFT_LED D8

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK,     // 0
                      ILI9341_DARKGREY,  // 1
                      ILI9341_RED,       // 2
                      ILI9341_LIGHTGREY};// 3

const int screen_height = 240;
const int screen_width = 320;

// Limited to 4 colors due to memory constraints
const int bits_per_pixel = 2;

ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, bits_per_pixel, palette);

const int cint = 1000;
String line[100];

WiFiClient client;
const char* ssid = "****";
const char* password = "****";

const char* station = "Jaegerstrasse";
 
const char* host = "wienerlinien.at";
const int httpPort = 80;

uint8_t lastT[3] = {30, 30, 30};
uint8_t t[3];

// converts the dBm to a range between 0 and 100%
uint8_t getWifiQuality()
{
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100)
  {
      return 0;
  }
  else if (dbm >= -50)
  {
      return 100;
  }
  else
  {
      return 2 * (dbm + 100);
  }
}

//draws the statusbar and the timeline
void drawStatic()
{
  uint8_t quality = getWifiQuality();
  gfx.setColor(D_RED);
  gfx.fillRect(0, 0, 320, 25);
  gfx.setColor(D_LIGHTGREY);
  for (uint8_t i = 0; i < 4; i++)
  {
    for (uint8_t j = 0; j < 4 * (i + 1); j++)
    {
      if (quality > i * 25 || j == 0)
      {
        gfx.setPixel(294 + 6 * i, 17 - j);
        gfx.setPixel(295 + 6 * i, 17 - j);
        gfx.setPixel(296 + 6 * i, 17 - j);
      }
    }
  }
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(320, 30, "0|");
  gfx.drawString(250, 30, "5|");
  gfx.drawString(180, 30, "10|");
  gfx.drawString(110, 30, "15|");
  gfx.drawString(40, 30, "20|");
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(160, 3, station);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(0, 3, "31er");
  gfx.setColor(D_DARKGREY);
  gfx.drawLine(317, 47, 317, 240);
  gfx.drawLine(247, 47, 247, 240);
  gfx.drawLine(177, 47, 177, 240);
  gfx.drawLine(107, 47, 107, 240);
  gfx.drawLine(37, 47, 37, 240);
}

void drawTram(uint16_t x, uint16_t y, bool barrierFree)
{
  if (barrierFree)
  {
    gfx.setColor(D_DARKGREY);
    gfx.drawXbm(x, y, tram_width, tram_height, ulf_dark_bits);
    gfx.setColor(D_LIGHTGREY);
    gfx.drawXbm(x, y, tram_width, tram_height, ulf_light_bits);
    gfx.setColor(D_RED);
    gfx.drawXbm(x, y, tram_width, tram_height, ulf_red_bits);
  }
  else
  {
    gfx.setColor(D_DARKGREY);
    gfx.drawXbm(x, y, tram_width, tram_height, tram_dark_bits);
    gfx.setColor(D_LIGHTGREY);
    gfx.drawXbm(x, y, tram_width, tram_height, tram_light_bits);
    gfx.setColor(D_RED);
    gfx.drawXbm(x, y, tram_width, tram_height, tram_red_bits);
  }
}

void drawTrams(bool barrierFree[3])
{
  bool statement1 = (lastT[0] == t[0]) && (lastT[1] == t[1]) && (lastT[2] == t[2]);
  bool statement2 = (abs(lastT[0] - t[0]) > 2) && (abs(lastT[1] - t[1]) > 2) && (abs(lastT[2] - t[2]) > 2);
  bool statement3 = (lastT[0] != 30) && (lastT[1] != 30) && (lastT[2] != 30);
  if ((statement1 || statement2) && statement3)
  {
    drawStatic();
    for (uint8_t n = 0; n < 3; n++)
    {
      drawTram(screen_width - (t[n] * 14) - tram_width, (n + 1) * 60, barrierFree[n]);
    }
    gfx.commit();
    gfx.clear();
    delay(10000);
  }
  else
  {
    uint8_t maxFrames = 14;
    uint8_t delayMs = 200;
    if(!statement3)
    {
      maxFrames = 40;
      delayMs = 10;
    }
    uint16_t tramPos[3] = {lastT[0] * 14, lastT[1] * 14, lastT[2] * 14};
    for (uint16_t n1 = 0; n1 < maxFrames; n1++)
    {
      drawStatic();
      for (uint8_t n2 = 0; n2 < 3; n2++)
      {
        tramPos[n2] += (t[n2] - lastT[n2]) * 14 / maxFrames;
        drawTram(screen_width - tramPos[n2] - tram_width, (n2 + 1) * 60, barrierFree[n2]);
      }
      gfx.commit();
      gfx.clear();
      delay(delayMs);
    }
  }
}

void setup()
{
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  gfx.init();
  gfx.setRotation(1);
  gfx.setFont(Orbitron_Bold_20);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("BimMonitor");
  WiFi.begin(ssid, password);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  for (uint16_t i = 0; i < 320; i += 4)
  {
    gfx.setColor(D_RED);
    gfx.fillRect(0, 0, i, 25);
    gfx.setColor(D_LIGHTGREY);
    gfx.drawString(160, 110, "Connecting to WiFi...");
    gfx.commit();
    gfx.clear();
  }
  while (WiFi.status() != WL_CONNECTED) yield();
}
 
void loop()
{
  if (!client.connect(host, httpPort))
  {
    return;
  }
 
  String url = "http://www.wienerlinien.at/ogd_realtime/monitor?rbl=2167&sender=k34RXngkrRDzfT2x";
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
               
  uint8_t i = 0;
  while (client.connected())
  {
    line[i] = client.readStringUntil('\n');
    i++;
  }
 
  DynamicJsonBuffer jsonBuffer(2000);
  JsonObject& data = jsonBuffer.parseObject(line[10]);
  if (!data.success())
  {
      return;
  }
  bool lineBarrierFree = data["data"]["monitors"][1]["lines"][0]["barrierFree"] == "true";
  bool barrierFree[3] = {lineBarrierFree, lineBarrierFree, lineBarrierFree};
  
  String barrierFreeS[3] = {data["data"]["monitors"][0]["lines"][0]["departures"]["departure"][0]["vehicle"]["barrierFree"],
                            data["data"]["monitors"][0]["lines"][0]["departures"]["departure"][1]["vehicle"]["barrierFree"],
                            data["data"]["monitors"][0]["lines"][0]["departures"]["departure"][2]["vehicle"]["barrierFree"]};
  
  for (uint8_t n = 0; n < 3; n++)
  {
    t[n] = atoi(data["data"]["monitors"][0]["lines"][0]["departures"]["departure"][n]["departureTime"]["countdown"]);
    if (barrierFreeS[n].length() > 0) barrierFree[n] = (barrierFreeS[n] == "true");
  }

  drawTrams(barrierFree);

  lastT[0] = t[0];
  lastT[1] = t[1];
  lastT[2] = t[2];
}
