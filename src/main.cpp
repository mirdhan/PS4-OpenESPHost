#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <FS.h>

struct Configuration
{
    char ssid[31];
    char password[63];
    IPAddress ip_address;
    IPAddress gateway;
    IPAddress subnet;
};

/* ESP8266 CONFIG FILE */
const char *DEFAULT_CONFIG_FILENAME = "/settings.json";

/* WEB SERVER CONFIG */
int DEFAULT_HTTP_PORT = 80;

/* DNS CONFIG */
int DEFAULT_DNS_PORT = 53;
int DEFAULT_DNS_TTL = 86400; // 24 hours

Configuration config;
AsyncWebServer webServer(DEFAULT_HTTP_PORT);
DNSServer dnsServer;

void loadConfiguration(const char *filename, Configuration &config)
{
    File file = SPIFFS.open(filename, "r");
    StaticJsonBuffer<384> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(file);

    if (!root.success())
        Serial.println("Failed to read file, using default configuration");

    /* Parse the JSON object to the Configuration Structure */
    strlcpy(config.ssid, root["ssid"], sizeof(config.ssid));
    strlcpy(config.password, root["password"], sizeof(config.password));
    config.ip_address.fromString(root["ip_address"].as<char *>());
    config.subnet.fromString(root["subnet"].as<char *>());
    config.gateway.fromString(root["gateway"].as<char *>());

    jsonBuffer.clear();
    file.close();
}

void setup()
{
    delay(1000);
    Serial.begin(9600);

    /* Loading general configuration */
    if (!SPIFFS.begin())
        Serial.println("Failed to mount SPIFFS");
    loadConfiguration(DEFAULT_CONFIG_FILENAME, config);

    /* Settings up Wi-Fi AP */
    WiFi.softAPConfig(config.ip_address, config.gateway, config.subnet);
    WiFi.softAP(config.ssid, config.password);

    /* Settings up DNS Server */
    dnsServer.setTTL(DEFAULT_DNS_TTL);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

    /* Redirect all domains to the local WebServer */
    dnsServer.start(DEFAULT_DNS_PORT, "*", config.ip_address);

    /* Settings up WebServer */
    webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    webServer.begin();
}

void loop()
{
    dnsServer.processNextRequest();
}
