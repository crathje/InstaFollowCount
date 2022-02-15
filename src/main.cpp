#include <Arduino.h>
#include "WiFi.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char *ssid = "ONE WIFI TO RULE THEM ALL";
const char *password = "One Mississippi, Two Mississippi";

WiFiClientSecure secureClient;
HTTPClient http;

long followers = -1;

void setup()
{
  Serial.begin(115200);
  Serial.println();
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

  // do not validate the chain since the certificates change quite often and I am too lazy for a CA check with time and so on
  secureClient.setInsecure();
}

void loop()
{
  int ret;
  // based on https://stackoverflow.com/questions/32407851/instagram-api-how-can-i-retrieve-the-list-of-people-a-user-is-following-on-ins
  // channel id is: 44601813942
  String followersUrl = "https://www.instagram.com/graphql/query/?query_hash=c76146de99bb02f6415203be841dd25a&variables=%7B%22id%22%3A44601813942%2C%22include_reel%22%3Atrue%2C%22fetch_mutual%22%3Atrue%2C%22first%22%3A50%7D";
  // hate 302 and 301 to login page
  followersUrl = "https://iot.ra.thje.net/tmp/twitter-api-follower.json";
  http.begin(secureClient, followersUrl);
  ret = http.GET();
  ESP_LOGI(TAG, "GET: %d", ret);

  if (ret == HTTP_CODE_OK)
  {
    DynamicJsonDocument followerDoc(2048);
    deserializeJson(followerDoc, http.getStream());
    followers = followerDoc["data"]["user"]["edge_followed_by"]["count"];
    Serial.printf("followers: %ld\n", followers);
  }
  else
  {
    ESP_LOGE(TAG, "HTTP Error: %d", ret);
  }
  sleep(10 * 60);
}