#include <Arduino.h>
#include "WiFi.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>

const char *ssid = "ONE WIFI TO RULE THEM ALL";
const char *password = "One Mississippi, Two Mississippi";
const char *channelname = "CatInTheDiceBag";
// Instagram Channel ID, look at https://instagram.com/<YOURCHANNELNAME>/channel/?__a=1 for the ID
const char *channelid = "44601813942";

int requests = 0;
int maxRequests = 5;

unsigned long previousMillis = 0;
unsigned long interval = 30000;

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
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 5);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println(F("WiFi Connecting to:"));
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.println(ssid);
#endif

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
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

#ifdef USECACHEDSERVICETHATWASNEVERINTENDEDASPUBLIC
int followersByCachedService(String channelname, long *followers)
{
  int ret = -1;
  // this is some quick 'n dirty way to cache the stuff and always provide a small json generated from
  // some sites with internal fallback.
  // easier to adapt later on in case any of the sites change -> change on place and all clients continue working
  // needs whitelisting for accounts in order to not flood my webspace that much in case this slips out of hand
  String followersUrl = "https://iot.ra.thje.net/instainfo/?channel=" + channelname;
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    DynamicJsonDocument followerDoc(2048);
    deserializeJson(followerDoc, http.getStream());
    long newfollowers = -1;
    newfollowers = followerDoc["followers"];
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
#endif

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

void loop()
{
  long unsigned currentMillis = millis();
  if(WiFi.status() == WL_CONNECTED) {
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
      }
    }
  #ifdef USECACHEDSERVICETHATWASNEVERINTENDEDASPUBLIC
    if (!updateSucceeded)
    {
      if (followersByCachedService(String(channelname), &followers) != 0)
      {
        ESP_LOGE(TAG, "Could not get via Cached Service");
      }
      else
      {
        updateSucceeded = true;
      }
    }
  #endif
    if (followers > -1)
    {
      Serial.printf("%10lu:: Followers: %ld\n", millis(), followers);
  #ifdef ST7789_DRIVER
      tft.setTextSize(1);
      tft.setCursor(0, 0);
      tft.setTextDatum(TC_DATUM); // Center text on x,y position
      tft.setFreeFont(&FreeSerifBold12pt7b);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      if (followers_pre == -1) // only clear screen on first successful retrieval, otherwise the black BG of the font is enogh redraw
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString(channelname, tft.width() / 2, 5, GFXFF);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        tft.drawString("followers", tft.width() / 2, 5 + tft.fontHeight(GFXFF), GFXFF);
      }
      if (followers_pre != followers)
      {
        tft.setTextSize(2);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setFreeFont(&FreeSerifBold18pt7b);
        tft.drawString(String(followers), tft.width() / 2, tft.height() - tft.fontHeight(GFXFF) + 20, GFXFF);
      }
  #endif
    }
    if (updateSucceeded || requests >= maxRequests)
    {
      sleep(30 * 60); // every 30min to overcome rate limit
      requests = 0;
    }
    // esp_sleep_enable_timer_wakeup(60UL * 1000000UL);
    // esp_deep_sleep_start();
  } else if (WiFi.status() != WL_CONNECTED && ((currentMillis - previousMillis > interval))) {
    ESP_LOGE(TAG, "Reconnecting to WiFi");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}