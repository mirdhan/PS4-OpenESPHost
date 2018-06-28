#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <FS.h>
// #include <string.h>

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
ESP8266WebServer webServer(DEFAULT_HTTP_PORT);
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

void serveWebsite()
{
    webServer.serveStatic("/", SPIFFS, "/index.html");
    /* Serve the entire data folder */
    webServer.serveStatic("/index.html", SPIFFS, "/index.html");
    webServer.serveStatic("/css/bootstrap.min.css", SPIFFS, "/css/bootstrap.min.css");
    webServer.serveStatic("/css/dashboard.min.css", SPIFFS, "/css/dashboard.min.css");
    webServer.serveStatic("/js/bootstrap.bundle.min.js", SPIFFS, "/js/bootstrap.bundle.min.js");
    webServer.serveStatic("/js/jquery.min.js", SPIFFS, "/js/jquery.min.js");
    webServer.serveStatic("/js/jailbreak.min.js", SPIFFS, "/js/jailbreak.min.js");
    webServer.serveStatic("/payloads/ftp.html", SPIFFS, "/payloads/ftp.html");
    webServer.serveStatic("/payloads/game-dumper.html", SPIFFS, "/payloads/game-dumper.html");
    webServer.serveStatic("/payloads/kernel-dumper.html", SPIFFS, "/payloads/kernel-dumper.html");
    webServer.serveStatic("/payloads/hen.html", SPIFFS, "/payloads/hen.html");
    webServer.serveStatic("/payloads/enable-vr.html", SPIFFS, "/payloads/enable-vr.html");
    webServer.serveStatic("/payloads/db-sg-backup.html", SPIFFS, "/payloads/db-sg-backup.html");
    webServer.serveStatic("/payloads/app-to-usb.html", SPIFFS, "/payloads/app-to-usb.html");

    /* Some how this is not working
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        const char *filename = dir.fileName().c_str();
        // Exclude configuration file
        if (!strcmp(DEFAULT_CONFIG_FILENAME, filename)
            continue;
        webServer.serveStatic(filename, SPIFFS, filename);
    } */
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
    serveWebsite();
    webServer.begin();
}

void loop()
{
    dnsServer.processNextRequest();
    webServer.handleClient();
}
