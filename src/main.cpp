#include <Arduino.h>
#include "WiFi.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "RTClib.h"
#include <TimeLib.h>
#ifdef ST7789_DRIVER
#include "images.h"
#endif

#ifdef USEWS2812
#include <FastLED.h>
#define NUM_LEDS 60
#define LEDS_PER_DIGIT 10
#define DATA_PIN 17
CRGB leds[NUM_LEDS];
#endif

const char *ssid = "ONE WIFI TO RULE THEM ALL";
const char *password = "One Mississippi, Two Mississippi";
const char *channelname = "CatInTheDiceBag";
// Instagram Channel ID, look at https://instagram.com/<YOURCHANNELNAME>/channel/?__a=1 for the ID
const char *channelid = "44601813942";

const char compile_date[] = __DATE__ " " __TIME__;
DateTime _BUILD_DATE_DATETIME = DateTime(F(__DATE__), F(__TIME__));

int requests = 0;
int maxRequests = 5;

unsigned long previousMillis = 0;
unsigned long interval = 30 * 1000;

WiFiClientSecure secureClient;
HTTPClient http;

#ifdef ST7789_DRIVER
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
#define GFXFF 1

//tft.pushImage(xx, 0, 48, 48, catinthedicebag48.data);
//pushImage messes up the colors with the default export, just draw manually for now
void pushImage(TFT_eSPI t, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
  for (int xi = 0; xi < w; xi++)
  {
    for (int yi = 0; yi < h; yi++)
    {
      t.drawPixel(x + xi, y + yi, data[yi * w + xi]);
    }
  }
}

void drawDice(TFT_eSPI t, uint16_t x, uint16_t y, uint8_t value)
{
  ESP_LOGV(TAG, "drawDice:: x:%3d y:%2d v:%d", x, y, value);
  if (value < 0 || value > 9)
  {
    t.fillRect(x, y, blankreddice.width, blankreddice.height, tft.color565(114, 202, 193));
    return;
  }

  pushImage(t, x, y, blankreddice.width, blankreddice.height, reddiceNumers[value].data);
#ifdef USEBLANKDICEANDWRITEONIT
  pushImage(t, x, y, blankreddice.width, blankreddice.height, blankreddice.data);
  t.setTextSize(1);
  t.setTextColor(TFT_WHITE);
  t.setFreeFont(&FreeSerifBold9pt7b);
  t.setTextDatum(CC_DATUM);
  t.drawString(String(value), x + blankreddice.width / 2 - 1, y + 20, GFXFF);
#endif
}

void drawDiceNumber(TFT_eSPI t, uint16_t x, uint16_t y, long value, uint8_t trim = 0)
{
  long v = 100000;
  int xx = x;
  if (trim)
  {
    while (v > value)
    {
      v = v / 10;
    }
  }
  for (; v > 0;)
  {
    long val = (value / v) % 10;
    if (value < v)
    {
      val = -1; // clear
    }
    drawDice(t, xx, y, val);
    xx += blankreddice.width;
    v = v / 10;
  }
}

#endif

#ifdef USEWS2812
// used to draw on
// https://www.led-genial.de/LED-Nixie-M-6-stelliger-Bausatz-inkl-LED-Basic-Controller
void drawLEDDigit(CRGB *l, uint16_t baseaddress, uint8_t value)
{
  ESP_LOGV(TAG, "drawLEDDigit:: baseaddress:%3d v:%d", baseaddress, value);
  // all black, 10 LEDs per digit
  for (int i = 0; i < LEDS_PER_DIGIT; i++)
  {
    l[baseaddress + i] = CRGB::Black;
    // l[baseaddress + i] = CRGB(1, 1, 1);
  }
  if (value < 0 || value > 9)
  {
    return;
  }

  const uint8_t digittoaddress[] = {5, 0, 6, 1, 7, 2, 8, 3, 9, 4}; // needs to be adapted accordingly
  l[baseaddress + digittoaddress[value] * 2 + 0] = CRGB::Red;
  l[baseaddress + digittoaddress[value] * 2 + 1] = CRGB::Blue;
  ESP_LOGV(TAG, "drawLEDDigit:: setting:%3d because of v:%d", baseaddress + digittoaddress[value] * 2 + 0, value);
  ESP_LOGV(TAG, "drawLEDDigit:: setting:%3d because of v:%d", baseaddress + digittoaddress[value] * 2 + 1, value);
}

void drawLEDNumber(CRGB *l, long value, uint8_t trim = 0, uint8_t leadingZeros = 0)
{
  long v = pow(10, (NUM_LEDS/LEDS_PER_DIGIT-1));
  int xx = 0;
  if (trim)
  {
    while (v > value)
    {
      v = v / 10;
    }
  }
  for (; v > 0;)
  {
    long val = (value / v) % 10;
    if (value < v && v > 1 && !leadingZeros)
    {
      val = -1; // clear
    }
    drawLEDDigit(l, xx, val);
    xx += LEDS_PER_DIGIT; // 10 LEDs per digit
    v = v / 10;
  }
}
#endif

long followers = -1;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  uint64_t chipid;
  chipid = ESP.getEfuseMac(); //The chip ID is essentially its MAC address(length: 6 bytes).

  Serial.println(F("===== CHIP INFO ====="));
  Serial.printf("ChipID:            %04X%08X\n", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  Serial.printf("CpuFreqMHz:        %3d\n", ESP.getCpuFreqMHz());
  Serial.printf("ChipRevision:      %3d\n", ESP.getChipRevision());
  Serial.printf("FreeHeap:          %3d\n", ESP.getFreeHeap());
  Serial.printf("CycleCount:        %3d\n", ESP.getCycleCount());
  Serial.printf("SdkVersion:        %s\n", ESP.getSdkVersion());
  Serial.printf("FlashChipSize:     %3d\n", ESP.getFlashChipSize());
  //Serial.printf("FlashChipRealSize: %3d\n", ESP.getFlashChipRealSize());
  Serial.printf("FlashChipSpeed:    %3d\n", ESP.getFlashChipSpeed());
  Serial.printf("FlashChipMode:     %3d\n", ESP.getFlashChipMode());
  Serial.println(F("===== PERIPHERY ====="));
#ifdef USEWS2812
  Serial.printf("LED DATA PIN:     %3d\n", DATA_PIN);
  Serial.printf("LED COUNT:        %3d\n", NUM_LEDS);
#endif
  Serial.println(F("===== CONFIG ====="));
  Serial.printf("Channel Name:      %s\n", channelname);
  Serial.printf("Channel ID:        %s\n", channelid);
  Serial.println(F("===== COMPILE DATE ====="));
  Serial.println(compile_date);
  Serial.println(F("===== STARTING ====="));

// init LEDs
#ifdef USEWS2812
  pinMode(DATA_PIN, OUTPUT);
  //FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS); // maybe use RGB?
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS); // my testing strip is GRB
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();
#endif

// init TFT
#ifdef ST7789_DRIVER
  tft.init();
  tft.setRotation(1);
  //  tft.fillScreen(TFT_BLACK);
  tft.fillScreen(tft.color565(114, 202, 193));
  // for (int x = 0; x < tft.width(); x += catinthedicebag48.width)
  for (int x = 0; x < catinthedicebag48.width; x += catinthedicebag48.width)
  {
    pushImage(tft, x, 0, catinthedicebag48.width, catinthedicebag48.height, catinthedicebag48.data);
  }
  tft.setCursor(0, 55);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK);
  tft.println(F("WiFi Connecting to:"));
  tft.setTextColor(TFT_PURPLE);
  tft.println(ssid);
#endif

  // init WiFi
  // WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  String hostName = "InstaCount-" + String(channelname);
  hostName = hostName.substring(0, min((unsigned int)hostName.length(), (unsigned int)32));
  Serial.printf("HostName: %s\n", hostName.c_str());
  WiFi.setHostname(hostName.c_str());
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to: %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
#ifdef ST7789_DRIVER
  tft.println(WiFi.localIP());
#endif

  // do not validate the chain since the certificates change quite often and I am too lazy for a CA check with time and so on
  secureClient.setInsecure();
  http.setUserAgent(F("Mozilla/5.0 (Linux; Android 11; LM-G900)"));
}

int followersByGraphQL(String channelid, long *followers)
{
  int ret = -1;
  // based on https://stackoverflow.com/questions/32407851/instagram-api-how-can-i-retrieve-the-list-of-people-a-user-is-following-on-ins
  String followersUrl = "https://www.instagram.com/graphql/query/?query_hash=c76146de99bb02f6415203be841dd25a&variables=%7B%22id%22%3A" + channelid + "%2C%22include_reel%22%3Atrue%2C%22fetch_mutual%22%3Atrue%2C%22first%22%3A50%7D";
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    DynamicJsonDocument followerDoc(2048);
    deserializeJson(followerDoc, http.getStream());
    long newfollowers = -1;
    newfollowers = followerDoc["data"]["user"]["edge_followed_by"]["count"];
    if (newfollowers > -1)
    {
      *followers = newfollowers;
    }
    http.end();
    return 0; // success
  }
  else
  {
    ESP_LOGE(TAG, "HTTP Error: %d", ret);
  }
  http.end();
  return -1;
}

int followersByAnonSite(String channelname, long *followers, String siteURL)
{
  int ret = -1;
  String followersUrl = siteURL + channelname;
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    // save ram and walk in chunks
    while (http.getStream().available())
    {
      String line = http.getStream().readStringUntil('\n');
      // search for the user items, might state Followers in posts like 'thanks to all my Followers'...
      if (line.indexOf("<li class=\"user__item\"") > 0 && line.indexOf("Followers") > 0)
      {
        line = line.substring(0, line.indexOf("Followers"));
        ESP_LOGI(TAG, "line: %s", line.c_str());
        line = line.substring(line.lastIndexOf(">") + 1);
        ESP_LOGI(TAG, "line: %s", line.c_str());
        line.replace(" ", "");
        ESP_LOGI(TAG, "line: %s", line.c_str());
        long newfollowers = line.toInt();
        if (newfollowers > -1)
        {
          *followers = newfollowers;
          return 0; // success
        }
        http.end();
        return -1;
      }
    }
  }
  http.end();
  return -1;
}

int followersByGreatFon(String channelname, long *followers)
{
  String lowercasename = channelname;
  lowercasename.toLowerCase();
  return followersByAnonSite(lowercasename, followers, "https://greatfon.com/v/");
}

int followersByDUMPOR(String channelname, long *followers)
{
  String lowercasename = channelname;
  lowercasename.toLowerCase();
  return followersByAnonSite(lowercasename, followers, "https://dumpor.com/v/");
}

int followersByAnonIGViewer(String channelname, long *followers)
{
  int ret = -1;
  String lowercasename = channelname;
  lowercasename.toLowerCase();
  String followersUrl = "https://www.anonigviewer.com/profile.php?u=" + lowercasename;
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    // save ram and discard a lot of pretext
    bool found = false;
    while (http.getStream().available() && !found)
    {
      String line = http.getStream().readStringUntil('\n');
      if (line.indexOf(">Posts<") > 0)
      {
        found = true;
      }
    }
    String all = "";
    // save ram and walk in chunks
    while (http.getStream().available())
    {
      String line = http.getStream().readStringUntil('\n');
      all += line;
      if (line.indexOf(">Followers<") > 0)
      {
        all = all.substring(0, all.indexOf(">Followers<"));
        ESP_LOGI(TAG, "all: %s", all.c_str());
        all = all.substring(0, all.indexOf("</span"));
        ESP_LOGI(TAG, "all: %s", all.c_str());
        all = all.substring(all.lastIndexOf(">") + 1);
        ESP_LOGI(TAG, "all: %s", all.c_str());
        all.replace(",", "");
        ESP_LOGI(TAG, "all: %s", all.c_str());
        long newfollowers = all.toInt();
        if (newfollowers > -1)
        {
          *followers = newfollowers;
          return 0; // success
        }
        http.end();
        return -1;
      }
    }
  }
  else
  {
    ESP_LOGE(TAG, "HTTP Error: %d", ret);
  }
  http.end();
  return -1;
}

void loop()
{
  long unsigned currentMillis = millis();
  if (WiFi.status() == WL_CONNECTED)
  {
    requests++;
    long followers_pre = followers;
    bool updateSucceeded = false;
    if (!updateSucceeded)
    {
      if (followersByGraphQL(String(channelid), &followers) != 0)
      {
        ESP_LOGE(TAG, "Could not get via GraphQL");
      }
      else
      {
        updateSucceeded = true;
        ESP_LOGI(TAG, "GraphQL succeeded");
      }
    }
    if (!updateSucceeded)
    {
      if (followersByGreatFon(String(channelname), &followers) != 0)
      {
        ESP_LOGE(TAG, "Could not get via GreatFon");
      }
      else
      {
        updateSucceeded = true;
        ESP_LOGI(TAG, "GreatFon succeeded");
      }
    }
    if (!updateSucceeded)
    {
      if (followersByDUMPOR(String(channelname), &followers) != 0)
      {
        ESP_LOGE(TAG, "Could not get via DUMPOR");
      }
      else
      {
        updateSucceeded = true;
        ESP_LOGI(TAG, "DUMPOR succeeded");
      }
    }
    if (!updateSucceeded)
    {
      if (followersByAnonIGViewer(String(channelname), &followers) != 0)
      {
        ESP_LOGE(TAG, "Could not get via AnonIGViewer");
      }
      else
      {
        updateSucceeded = true;
        ESP_LOGI(TAG, "AnonIGViewer succeeded");
      }
    }
    if (followers > -1)
    {
      Serial.printf("%10lu:: Followers: %ld\n", millis(), followers);

#ifdef ST7789_DRIVER
      tft.setTextSize(1);
      tft.setCursor(0, 0);
      if (followers_pre == -1) // only clear screen on first successful retrieval, otherwise the BG of the font is enogh redraw in case numbers do not get smaller -> don't loose followers :-)
      {
        tft.setTextDatum(TC_DATUM); // Top Center Text
        tft.setFreeFont(&FreeSerifBold12pt7b);
        //tft.fillScreen(TFT_BLACK);
        // clear lower part where boot message have been
        tft.fillRect(0, 48, tft.width(), tft.height() - 48, tft.color565(114, 202, 193));
        tft.setTextColor(TFT_BLUE);
        tft.drawString(channelname, (tft.width() - catinthedicebag48.width) / 2 + catinthedicebag48.width, 3, GFXFF);
        tft.setTextColor(TFT_GOLD);
        tft.drawString("followers", (tft.width() - catinthedicebag48.width) / 2 + catinthedicebag48.width, 24, GFXFF);
      }
      if (followers_pre != followers)
      {
        tft.setTextColor(TFT_RED, tft.color565(114, 202, 193));
        // tft.setTextSize(2);
        // tft.setFreeFont(&FreeSerifBold18pt7b);
        // tft.drawString(String(followers), tft.width() / 2, 78, GFXFF);
        tft.setTextFont(7);
        tft.setTextDatum(TC_DATUM); // Center text on x position
        tft.drawString(String(followers), tft.width() / 2, 83);

        // drawDiceNumber(tft, 0, 48, followers);
        drawDiceNumber(tft, (tft.width() - String(followers).length() * blankreddice.width) / 2, 48, followers, 1);
      }
#endif

#ifdef USEWS2812
      drawLEDNumber(leds, followers);
      FastLED.show();
      /*
      // debug drawing the LEDs average light to get an idea which LED should be lit
      for (int i = 0; i < NUM_LEDS; i++)
      {
        Serial.printf("%2X", leds[i].getAverageLight());
      }
      Serial.println();
      */
#endif
    }
    if (updateSucceeded || requests >= maxRequests)
    {
      sleep(30 * 60); // every 30min to overcome rate limit
      requests = 0;
    }
    // esp_sleep_enable_timer_wakeup(60UL * 1000000UL);
    // esp_deep_sleep_start();
  }
  else if (WiFi.status() != WL_CONNECTED && ((currentMillis - previousMillis > interval)))
  {
    ESP_LOGE(TAG, "WiFi connection lost");
    WiFi.disconnect();
    ESP_LOGI(TAG, "Reconnecting to WiFi");
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}