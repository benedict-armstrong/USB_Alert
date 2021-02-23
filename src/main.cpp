#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include "WiFiClientSecure.h"
#include <Adafruit_NeoPixel.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>


#define PIN 4
#define NUMPIXELS 2

struct Config
{
  char url1[256] = "";
  char url2[256] = "";
  char refresh[10] = "100000";
};

Config config; 

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void getConfig(){
  if (LittleFS.exists("/config.json")) {
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = LittleFS.open("/config.json", "r");
    
    Serial.println("opening config file");

    StaticJsonDocument<384> json;
    DeserializationError error = deserializeJson(json, configFile);
    if (error) {
      Serial.println(F("Failed to read file, using default configuration"));
    }

    Serial.print("Saved URL1: ");
    Serial.println(json["url1"].as<String>());
    Serial.print("Saved URL2: ");
    Serial.println(json["url2"].as<String>());
    Serial.print("Refresh: ");
    Serial.println(json["refresh"].as<String>());

    strlcpy(config.url1, json["url1"] | "", sizeof(config.url1));
    strlcpy(config.url2, json["url2"] | "", sizeof(config.url2));
    strlcpy(config.refresh, json["refresh"] | "100000", sizeof(config.refresh)); 


    configFile.close();

  } else {
    Serial.println("No config found");
  }
  //end read
}

String get_status(String host) {

  HTTPClient http;
  WiFiClientSecure client;
  String payload;

  client.setInsecure(); 
  client.connect(host, 443);
  http.begin(client, host);

  if (http.GET() == HTTP_CODE_OK)    
    return payload = http.getString(); 
  else
    return "error";
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  LittleFS.begin();

  getConfig();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_token1("url1", "Url 1", config.url1, 256);
  WiFiManagerParameter custom_token2("url2", "Url 2", config.url2, 256);
  WiFiManagerParameter refresh("refresh", "Refresh time", config.refresh, 10);

  wifiManager.addParameter(&custom_token1);
  wifiManager.addParameter(&custom_token2);
  wifiManager.addParameter(&refresh);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect("CC Agent")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("connected...");

  String host1 = custom_token1.getValue();
  String host2 = custom_token2.getValue();
  String ref = refresh.getValue();

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    File file = LittleFS.open("/config.json", "w");

    if (!file) {
      Serial.println(F("Failed to create file"));
      return;
    }

    StaticJsonDocument<384> json;

    json["url1"] = host1;
    json["url2"] = host2;
    json["refresh"] = ref;

    // Serialize JSON to file
    if (serializeJson(json, file) == 0) {
      Serial.println(F("Failed to write to file"));
    }
    // Close the file
    file.close();
  }


  pixels.begin();

  String status1 = "No Url (1)";
  String status2 = "No Url (2)";

  if (host1.length() > 0 ) {
    Serial.println("Starting 1st HTTPS Request:");
    Serial.println(host1);
    status1 = get_status(host1);
  }

  Serial.println(status1);

  if (host2.length() > 0) {
    Serial.println("Starting 2st HTTPS Request");
    Serial.println(host2);
    status2 = get_status(host2);
  }

  Serial.println(status2);
  
  pixels.clear();

  pixels.setPixelColor(0, pixels.Color(100, 0, 0));
  pixels.setPixelColor(1, pixels.Color(100, 0, 0));

  if (status1 == "OK") {
    pixels.setPixelColor(0, pixels.Color(0, 100, 0));
  }

  if (status2 == "OK") {
    pixels.setPixelColor(1, pixels.Color(0, 100, 0));
  }

  pixels.show();

  Serial.println("Done. Going to sleep!");
  Serial.println(atoi( ref.c_str() ));
  ESP.deepSleep(atoi( ref.c_str() ));

}

void loop() {}