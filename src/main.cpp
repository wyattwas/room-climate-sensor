#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SensirionI2cScd30.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Wire.h>
#include "LightColor.h"

#ifndef WIFI
#define WIFI_SSID "IoT-WLAN"
#define WIFI_PSK "dalmatiner"
#define MQTT_IP "172.20.200.60"
#endif

#define TFT_DC D4
#define TFT_CS D8
#define TFT_RST D3

#define PIEZO_PIN D6

#define SDA_PIN D2
#define SCL_PIN D1

const char* const ssid = WIFI_SSID;
const char* const password = WIFI_PSK;
const char* const mqtt_server = MQTT_IP;

constexpr int co2High = 2000;
constexpr int co2Mid = 1000;

char macStr[18];

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long currentWaitTime = 0;
unsigned long loopStartTime = 0;
unsigned long loopWaitTime = 0;

void setup_wifi(bool full_display = false);
void setup_mqtt(bool full_display = false);
void set_display(bool connection_text = false);
void write_head();
void write_connection();
void write_co2(float value, bool write_head = false);
void write_humid(float value, bool write_head = false);
void write_temp(float value, bool write_head = false);

SensirionI2cScd30 scd30;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

float current_temp = 0.0;
float current_humid = 0.0;
float current_co2 = 0.0;
float old_temp = 0.0;
float old_humid = 0.0;
float old_co2 = 0.0;
LightColor color = LightColor::OFF;
LightColor old_color = LightColor::OFF;

void setup()
{
    Serial.begin(9600);

    pinMode(PIEZO_PIN, OUTPUT);

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    setup_wifi(true);
    setup_mqtt(true);
    Wire.begin(SDA_PIN, SCL_PIN);
    scd30.begin(Wire, SCD30_I2C_ADDR_61);

    scd30.forceRecalibration(800);
    scd30.activateAutoCalibration(true);
    scd30.startPeriodicMeasurement(0);
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        setup_wifi();
    }

    if (!client.connected())
    {
        setup_mqtt();
    }

    scd30.blockingReadMeasurementData(current_co2, current_temp, current_humid);

    char topic[50];
    strcpy(topic, "sensoren/daten/");
    strncat(topic, macStr, strlen(macStr));
    DynamicJsonDocument doc(1024);

    if (current_co2 > co2High)
    {
        color = LightColor::RED;
    }
    else if (current_co2 <= co2High && current_co2 > co2Mid)
    {
        color = LightColor::YELLOW;
    }
    else
    {
        color = LightColor::GREEN;
    }

    Serial.println("Current values:");
    Serial.print("CO2: ");
    Serial.println(current_co2);
    Serial.print("Temperature: ");
    Serial.println(current_temp);
    Serial.print("Humidity: ");
    Serial.println(current_humid);

    doc["device_id"] = macStr;
    doc["co2"] = current_co2;
    doc["temperature"] = current_temp;
    doc["humidity"] = current_humid;

    char mqtt_message[128];
    serializeJson(doc, mqtt_message);

    if (millis() - loopStartTime >= loopWaitTime)
    {
        client.publish(topic, mqtt_message, true);
        Serial.print("MQTT publish ");
        Serial.println(mqtt_message);
        loopWaitTime = 5 * 60 * 1000;
        loopStartTime = millis();
    }

    set_display();
}

void setup_wifi(bool full_display)
{
    Serial.print("Connecting to ");
    Serial.println(ssid);

    if (full_display)
    {
        color = LightColor::BLUE;
        set_display(true);
    }
    else
    {
        tft.fillRect(0, 0, 160, 5, ST7735_BLUE);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    byte mac[6];
    WiFi.macAddress(mac);
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    color = LightColor::OFF;

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(macStr);
}

void setup_mqtt(bool full_display)
{
    Serial.print("Connecting to MQTT Server at ");
    Serial.println(mqtt_server);

    if (full_display)
    {
        color = LightColor::BLUE;
        set_display(true);
    }
    else
    {
        tft.fillRect(0, 0, 160, 5, ST7735_BLUE);
    }

    client.setServer(mqtt_server, 1883);

    while (!client.connected())
    {
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str()))
        {
            Serial.println("MQTT connected");
        }
        else
        {
            delay(500);
            Serial.print(".");
        }
    }

    color = LightColor::OFF;
}

void set_display(bool connection_text)
{
    int text_color = 0x0000;
    int replace_color = 0x0000;

    if (color == LightColor::BLUE || color == LightColor::RED)
    {
        text_color = 0xffff;
    }

    switch (color)
    {
    case LightColor::RED:
        replace_color = ST7735_RED;
        break;
    case LightColor::GREEN:
        replace_color = ST7735_GREEN;
        break;
    case LightColor::YELLOW:
        replace_color = ST7735_YELLOW;
        break;
    case LightColor::BLUE:
        replace_color = ST7735_BLUE;
        break;
    case LightColor::OFF:
        replace_color = ST7735_WHITE;
        break;
    }

    if (color != old_color)
    {
        switch (color)
        {
        case LightColor::RED:
            tft.fillScreen(ST7735_RED);
            break;
        case LightColor::GREEN:
            tft.fillScreen(ST7735_GREEN);
            break;
        case LightColor::YELLOW:
            tft.fillScreen(ST7735_YELLOW);
            break;
        case LightColor::BLUE:
            tft.fillScreen(ST7735_BLUE);
            break;
        case LightColor::OFF:
            tft.fillScreen(ST7735_WHITE);
            break;
        }

        tft.setTextColor(text_color);
        write_head();

        if (connection_text)
        {
            write_connection();
        }
        else
        {
            write_co2(current_co2, true);
            write_humid(current_humid, true);
            write_temp(current_temp, true);
        }
    }
    else
    {
        tft.fillRect(0, 0, 160, 5, replace_color);

        if (old_co2 != current_co2)
        {
            tft.setTextColor(replace_color);
            write_co2(old_co2);
            tft.setTextColor(text_color);
            write_co2(current_co2);
        }
        if (old_humid != current_humid)
        {
            tft.setTextColor(replace_color);
            write_humid(old_humid);
            tft.setTextColor(text_color);
            write_humid(current_humid);
        }
        if (old_temp != current_temp)
        {
            tft.setTextColor(replace_color);
            write_temp(old_temp);
            tft.setTextColor(text_color);
            write_temp(current_temp);
        }
    }

    old_color = color;
    old_co2 = current_co2;
    old_humid = current_humid;
    old_temp = current_temp;
}

void write_head()
{
    tft.setTextSize(2);
    tft.setCursor(16, 10);
    tft.print("Klimamesser");
}

void write_connection()
{
    tft.setCursor(10, 40);
    tft.setTextSize(1);
    tft.print("IP: ");
    tft.print(WiFi.localIP());
    tft.setCursor(10, 50);
    tft.print("MAC: ");
    tft.print(WiFi.macAddress());
}

void write_co2(float value, bool write_head)
{
    if (write_head)
    {
        tft.setCursor(10, 35);
        tft.setTextSize(1);
        tft.print("CO2: ");
    }
    tft.setCursor(10, 45);
    tft.setTextSize(2);
    tft.print(value);
    tft.print(" ppm");
}

void write_humid(float value, bool write_head)
{
    if (write_head)
    {
        tft.setCursor(10, 65);
        tft.setTextSize(1);
        tft.print("Humid: ");
    }
    tft.setCursor(10, 75);
    tft.setTextSize(2);
    tft.print(value);
    tft.print(" %");
}

void write_temp(float value, bool write_head)
{
    if (write_head)
    {
        tft.setCursor(10, 95);
        tft.setTextSize(1);
        tft.print("Temp: ");
    }
    tft.setCursor(10, 105);
    tft.setTextSize(2);
    tft.print(value);
    tft.print(" Grad C");
}
