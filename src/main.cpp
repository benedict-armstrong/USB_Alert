#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include "WiFiClientSecure.h"
#include <Adafruit_NeoPixel.h>
#include <Ticker.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>


#define PIN 4
#define NUMPIXELS 2

#define TRIGGER_PIN 2

struct Config
{
  char url1[256] = "url for left eye";
  char url2[256] = "url for right eye";
  char refresh[10] = "100000";
};

Config config;
Ticker ticker;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//WiFiManager
WiFiManager wifiManager;

bool portalRunning = false;

//flag for saving data
bool shouldSaveConfig = false;

void toggle(int led_num, uint32_t color) {
  //toggle state
  if (pixels.getPixelColor(led_num) == 0) {
    pixels.clear();
    pixels.setPixelColor(led_num, color);
  } else {
    pixels.setPixelColor(led_num, pixels.Color(0,0,0));
  }
  pixels.show();
}

void config_blink() {
  toggle(0, pixels.Color(0,0,15));
}

void white1() {
  toggle(0, pixels.Color(15,15,15));
}

void white2() {
  toggle(1, pixels.Color(15,15,15));
}

void yellow() {
  toggle(0, pixels.Color(15,15,0));
  toggle(1, pixels.Color(15,15,0));
}


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  portalRunning = false;
  shouldSaveConfig = true;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  portalRunning = true;
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  toggle(0, pixels.Color(0,0,15));
  ticker.attach(0.6, config_blink);
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

void force_configuration() {
  delay(500);
  if (digitalRead(TRIGGER_PIN) == LOW) {
    if(!portalRunning){
        Serial.println("Button Pressed, Starting Portal");
        wifiManager.startConfigPortal();
        portalRunning = true;
      }
    }
  }


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay (500);

  //attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), force_configuration, FALLING);

  LittleFS.begin();

  pixels.begin();

  toggle(0, pixels.Color(15,15,0));
  ticker.attach(0.8, yellow);

  getConfig();

  pixels.setPixelColor(0, pixels.Color(15,15,15));
  pixels.show();
  delay(500);

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


  String status1 = "No Url (1)";
  String status2 = "No Url (2)";

  // Request to URL 1v
  ticker.attach(0.8, white1);

  if (host1.length() > 0 ) {
    Serial.println("Starting 1st HTTPS Request:");
    Serial.println(host1);
    status1 = get_status(host1);
  }

  Serial.println(status1);

  // Request to URL 2

  ticker.attach(0.8, white2);

  if (host2.length() > 0) {
    Serial.println("Starting 2st HTTPS Request");
    Serial.println(host2);
    status2 = get_status(host2);
  }

  Serial.println(status2);
  
  // Stop blinking
  ticker.detach();

  pixels.clear();

  pixels.setPixelColor(0, pixels.Color(50, 0, 0));
  pixels.setPixelColor(1, pixels.Color(50, 0, 0));

  if (status1 == "OK") {
    pixels.setPixelColor(0, pixels.Color(0, 50, 0));
  }

  if (status2 == "OK") {
    pixels.setPixelColor(1, pixels.Color(0, 50, 0));
  }

  pixels.show();
  Serial.println("Done. Going to sleep!");
  Serial.println(atoi( ref.c_str() ));
  ESP.deepSleep(atoi( ref.c_str() ));

}

void loop() {}