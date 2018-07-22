#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <FS.h>

struct Configuration
{
    const char *ssid;
    const char *password;
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
        Serial.println("Failed to read file");

    /* Parse the JSON object to the Configuration Structure */
    config.ssid = root["ssid"].as<const char *>();
    config.password = root["password"].as<const char *>();
    config.ip_address.fromString(root["ip_address"].as<char *>());
    config.subnet.fromString(root["subnet"].as<char *>());
    config.gateway.fromString(root["gateway"].as<char *>());

    jsonBuffer.clear();
    file.close();
}

void saveConfiguration(const char *filename, const Configuration &config)
{
    // Open file for writing
    File file = SPIFFS.open(filename, "w"); // Opens a file for writing only. Overwrites the file if the file exists. If the file does not exist, creates a new file for writing.
    if (!file)
    {
        Serial.println("Failed to create file");
        return;
    }

    StaticJsonBuffer<384> jsonBuffer;

    // Parse the root object
    JsonObject &root = jsonBuffer.createObject();

    // Set the values
    root["ssid"] = config.ssid;
    root["password"] = config.password;
    root["ip_address"] = config.ip_address.toString();
    root["subnet"] = config.subnet.toString();
    root["gateway"] = config.gateway.toString();

    // Serialize JSON to file
    if (root.printTo(file) == 0)
    {
        Serial.println("Failed to write to file");
    }

    jsonBuffer.clear();
    file.close();
}

void setup()
{
    delay(1000);
    Serial.begin(9600);

    /* Loading general configuration */
    if (!SPIFFS.begin())
    {
        Serial.println("Failed to mount SPIFFS");
    }

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

    /* Implement restart route */
    webServer.on("/esp8266/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        ESP.restart();
    });

    /* Implement reset route */
    webServer.on("/esp8266/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        ESP.reset();
    });

    /* Implement information route */
    webServer.on("/esp8266/information", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        /* Set values */
        root["boot_mode"] = ESP.getBootMode();
        root["boot_version"] = ESP.getBootVersion();
        root["chip_id"] = ESP.getChipId();
        root["core_version"] = ESP.getCoreVersion();
        root["cpu_freq"] = ESP.getCpuFreqMHz();
        root["cycle_count"] = ESP.getCycleCount();
        root["flash_chip_id"] = ESP.getFlashChipId();
        root["flash_chip_mode"] = ESP.getFlashChipMode();
        root["flash_chip_real_size"] = ESP.getFlashChipRealSize();
        root["flash_chip_size"] = ESP.getFlashChipSize();
        root["flash_chip_size_by_chip_id"] = ESP.getFlashChipSizeByChipId();
        root["flash_chip_speed"] = ESP.getFlashChipSpeed();
        root["free_heap"] = ESP.getFreeHeap();
        root["free_sketch_space"] = ESP.getFreeSketchSpace();
        root["full_version"] = ESP.getFullVersion();
        root["reset_info"] = ESP.getResetInfo();
        root["reset_reason"] = ESP.getResetReason();
        root["sdk_version"] = ESP.getSdkVersion();
        root["sketch_md5"] = ESP.getSketchMD5();
        root["sketch_size"] = ESP.getSketchSize();
        root["vcc"] = ESP.getVcc();
        String response;
        // Serialize JSON to Sring to send it
        root.printTo(response);
        jsonBuffer.clear();

        return request->send(200, "application/json", response);
    });

    /* Implement update configuration route */
    webServer.on("/settings/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        /* Validation Rules */
        if (!request->hasParam("ssid", true))
        {
            return request->send(400, "text/plain", "SSID parameter is required");
        }
        if (!request->hasParam("password", true))
        {
            return request->send(400, "text/plain", "Password parameter is required");
        }
        if (!request->hasParam("ip_address", true))
        {
            return request->send(400, "text/plain", "IP Address parameter is required");
        }
        if (!request->hasParam("subnet", true))
        {
            return request->send(400, "text/plain", "Subnet parameter is required");
        }
        if (!request->hasParam("gateway", true))
        {
            return request->send(400, "text/plain", "Gateway parameter is required");
        }

        /* If we made it so far, we can modify the configuration and save it */
        AsyncWebParameter *p = request->getParam("ssid", true);
        if (p->value().length() == 0)
        {
            return request->send(400, "text/plain", "SSID value is required");
        }
        config.ssid = p->value().c_str();

        /* No verification for password because Wi-Fi Ap can be open */
        p = request->getParam("password", true);
        config.password = p->value().c_str();

        p = request->getParam("ip_address", true);
        if (!config.ip_address.fromString(p->value()))
        {
            return request->send(400, "text/plain", "IP Address is not valid");
        }

        p = request->getParam("subnet", true);
        if (!config.subnet.fromString(p->value()))
        {
            return request->send(400, "text/plain", "Subnet is not valid");
        }

        p = request->getParam("gateway", true);
        if (!config.gateway.fromString(p->value()))
        {
            return request->send(400, "text/plain", "Gateway is not valid");
        }

        saveConfiguration(DEFAULT_CONFIG_FILENAME, config);
        return request->send(200, "text/plain", "Configuration updated");
    });
    webServer.begin();
}

void loop()
{
    dnsServer.processNextRequest();
}
