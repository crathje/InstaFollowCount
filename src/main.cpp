#include <Arduino.h>
#include "WiFi.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>

const char *ssid = "ONE WIFI TO RULE THEM ALL";
const char *password = "One Mississippi, Two Mississippi";

WiFiClientSecure secureClient;
HTTPClient http;

#ifdef ST7789_DRIVER
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
#define GFXFF 1
#endif

long followers = -1;

void setup()
{
  Serial.begin(115200);
  Serial.println();

// init TFT
#ifdef ST7789_DRIVER
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4, 5);
  tft.println(F("WiFi Connecting"));
#endif

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
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
}

void loop()
{
  int ret;
  // based on https://stackoverflow.com/questions/32407851/instagram-api-how-can-i-retrieve-the-list-of-people-a-user-is-following-on-ins
  // channel id is: 44601813942
  String followersUrl = "https://www.instagram.com/graphql/query/?query_hash=c76146de99bb02f6415203be841dd25a&variables=%7B%22id%22%3A44601813942%2C%22include_reel%22%3Atrue%2C%22fetch_mutual%22%3Atrue%2C%22first%22%3A50%7D";
  // hate 302 and 301 to login page due to rate limit for testing locally
  if (ssid[0] == 'E' && ssid[strlen(ssid) - 1] == 't')
  {
    followersUrl = "https://iot.ra.thje.net/tmp/twitter-api-follower.json";
  }
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    DynamicJsonDocument followerDoc(2048);
    deserializeJson(followerDoc, http.getStream());
    followers = followerDoc["data"]["user"]["edge_followed_by"]["count"];
    Serial.printf("followers: %ld\n", followers);
#ifdef ST7789_DRIVER
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.setTextDatum(TC_DATUM); // Centre text on x,y position
    tft.setFreeFont(&FreeSerifBold12pt7b);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("CatInTheDiceBag", tft.width() / 2, 5, GFXFF);
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.drawString("followers", tft.width() / 2, 5 + tft.fontHeight(GFXFF), GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setFreeFont(&FreeSerifBold18pt7b);
    tft.drawString(String(followers), tft.width() / 2, tft.height() - tft.fontHeight(GFXFF) + 20, GFXFF);
#endif
  }
  else
  {
    ESP_LOGE(TAG, "HTTP Error: %d", ret);
  }
  sleep(10 * 60);
}