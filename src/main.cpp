#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <SensirionI2cScd30.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Wire.h>
#include "LightColor.h"

#ifndef STASSID
#define STASSID "IoT-WLAN"
#define STAPSK "dalmatiner"
#define STAMQTT "172.20.200.60"
#endif

#define TFT_DC D4
#define TFT_CS D8
#define TFT_RST D3

#define PIEZO_PIN D6

#define SDA_PIN D2
#define SCL_PIN D1

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = STAMQTT;

int co2VeryHigh = 2500;
int co2High = 2000;
int co2Mid = 1500;
int co2CalibValue = 500;

char macStr[18];

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long currentWaitTime = 0;
unsigned long loopStartTime = 0;
unsigned long loopWaitTime = 0;

//  initialsing functuions
void setup_wifi();
void setup_mqtt();
void set_light(LightColor colorl);
void set_display();

//  declaring sensors
SensirionI2cScd30 scd30;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

//  SCD30
float current_temp = 0.0;
float current_humid = 0.0;
float current_co2 = 0.0;
LightColor color = LightColor::OFF;

void setup()
{
    Serial.begin(9600);

    pinMode(PIEZO_PIN, OUTPUT);

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    setup_wifi();
    setup_mqtt();
    Wire.begin(SDA_PIN, SCL_PIN);
    scd30.begin(Wire, SCD30_I2C_ADDR_61);
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

    if (current_co2 > co2VeryHigh)
    {
        set_light(LightColor::RED);
    }
    else if (current_co2 <= co2VeryHigh && current_co2 > co2High)
    {
        set_light(LightColor::RED);
    }
    else if (current_co2 <= co2High && current_co2 > co2Mid)
    {
        set_light(LightColor::YELLOW);
    }
    else
    {
        set_light(LightColor::GREEN);
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
}

void setup_wifi()
{
    Serial.print("Connecting to ");
    Serial.println(ssid);
    set_light(LightColor::BLUE);
    set_display();

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

    set_light(LightColor::OFF);
    set_display();

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(macStr);
}

void setup_mqtt()
{
    Serial.print("Connecting to MQTT Server at ");
    Serial.println(mqtt_server);
    set_light(LightColor::BLUE);
    set_display();

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

    set_light(LightColor::OFF);
    set_display();
}

void set_light(LightColor colorl)
{
    color = colorl;
}

void set_display()
{
    switch (color)
    {
    case LightColor::RED:
        tft.fillScreen(ST7735_RED);
        tft.fillRect(5, 5, tft.width() - 10, tft.height() - 10, ST7735_WHITE);
        break;
    case LightColor::GREEN:
        tft.fillScreen(ST7735_GREEN);
        tft.fillRect(5, 5, tft.width() - 10, tft.height() - 10, ST7735_WHITE);
        break;
    case LightColor::YELLOW:
        tft.fillScreen(ST7735_YELLOW);
        tft.fillRect(5, 5, tft.width() - 10, tft.height() - 10, ST7735_WHITE);
        break;
    case LightColor::BLUE:
        tft.fillScreen(ST7735_BLUE);
        tft.fillRect(5, 5, tft.width() - 10, tft.height() - 10, ST7735_WHITE);
        break;
    case LightColor::OFF:
        tft.fillScreen(ST7735_WHITE);
        break;
    }

    tft.setTextSize(2);

    tft.setCursor(20, 10);
    tft.setTextColor(0x0000);
    tft.print("Klimamesser");

    tft.setTextSize(1);

    tft.setCursor(10, 40);
    tft.print("CO2: ");
    tft.print(current_co2);
    tft.print(" ppm");
    tft.setCursor(10, 50);
    tft.print("Luftfeuchtigkeit: ");
    tft.print(current_humid);
    tft.print(" %");
    tft.setCursor(10, 60);
    tft.print("Temperatur: ");
    tft.print(current_temp);
    tft.print(" Grad C");

    tft.setCursor(10, 100);
    tft.print("IP: ");
    tft.print(WiFi.localIP());
    tft.setCursor(10, 110);
    tft.print("MAC: ");
    tft.print(WiFi.macAddress());
}