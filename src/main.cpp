#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SensirionI2CScd4x.h>
#include <SensirionI2CSht4x.h>
#include <SensirionI2CSvm41.h>
#include "State.h"

#ifndef STASSID
#define STASSID ""
#define STAPSK ""
#define STAMQTT ""
#define STAOTAPSK ""
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = STAMQTT;
const char* ota_password = STAOTAPSK;

//  LED pins
constexpr int RGB_LED_BLUE_PIN = D3;
constexpr int RGB_LED_RED_PIN = D4;
constexpr int RGB_LED_GREEN_PIN = D5;
constexpr int LED_RED_PIN = D7;
// Button pin
constexpr int BUTTON_PIN = D6;
// Piezo pin
constexpr int PIEZO_PIN = D0;

int buttonState = 0;
int lastButtonState = 0;

int co2VeryHigh = 2500;
int co2High = 2000;
int co2Mid = 1500;
int co2CalibValue = 500;

char macStr[18];

WiFiClient espClient;
PubSubClient client(espClient);

State current_state = State::Operation;

unsigned long stepStartTime = 0;
unsigned long currentWaitTime = 0;

unsigned long loopStartTime = 0;
unsigned long loopWaitTime = 0;

//  initialsing functuions
void setup_wifi();
void setup_ota();
void setup_mqtt();
void callback(char* topic, byte* payload, unsigned int length);
void pin_blink(int pin, long interval, int pull_resistor);
void pin_blink(int pin1, int pin2, long interval, int pull_resistor);
void pin_blink_diff_times(int pin1, long interval_wait, long interval_on, int pull_resistor);

//  declaring sensors
SensirionI2cSht4x sht4x;
SensirionI2cScd4x scd4x;
SensirionI2CSvm41 svm40;

//  SHT41
float_t current_sht4x_temp_float = 0;
float_t current_sht4x_humid_float = 0;
uint32_t current_sht4x_humid = 0;

//  SVM40
int16_t current_svm40_temp = 0;
int16_t current_svm40_humid = 0;
int16_t current_svm40_voc = 0;

//  SCD41
float_t current_scd41_temp_float = 0.0f;
float_t current_scd41_humid_float = 0.0f;
uint16_t current_scd41_co2 = 0;
uint16_t frcCorrection = 0;

void setup()
{
    Serial.begin(9600);

    pinMode(RGB_LED_RED_PIN, OUTPUT);
    pinMode(RGB_LED_GREEN_PIN, OUTPUT);
    pinMode(RGB_LED_BLUE_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(PIEZO_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);

    setup_wifi();
    setup_ota();
    setup_mqtt();
    Wire.begin();

    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    sht4x.begin(Wire, SHT40_I2C_ADDR_44);
    svm40.begin(Wire);

    scd4x.startPeriodicMeasurement();
    scd4x.setAutomaticSelfCalibrationEnabled(1);
    svm40.startMeasurement();
}

void loop()
{
    ArduinoOTA.handle();
    buttonState = digitalRead(BUTTON_PIN);
    if (buttonState != lastButtonState && !buttonState)
    {
        current_state = State::Calibration_Start;
        Serial.println("Changed state to calibrating");
    }
    lastButtonState = buttonState;

    if (WiFi.status() != WL_CONNECTED)
    {
        setup_wifi();
    }

    if (!client.connected())
    {
        setup_mqtt();
    }

    client.loop();

    char topic[50];
    strcpy(topic, "co2melder/data/");
    strncat(topic, macStr, strlen(macStr));
    DynamicJsonDocument doc(1024);

    switch (current_state)
    {
    case State::Operation:
        scd4x.readMeasurement(current_scd41_co2, current_scd41_temp_float, current_scd41_humid_float);
        sht4x.measureHighPrecision(current_sht4x_temp_float, current_sht4x_humid_float);

        if (current_scd41_co2 > co2VeryHigh)
        {
            pin_blink(RGB_LED_RED_PIN, 1000, HIGH);
            pin_blink_diff_times(PIEZO_PIN, 3000, 1000, LOW);
            digitalWrite(RGB_LED_GREEN_PIN, HIGH);
            digitalWrite(RGB_LED_BLUE_PIN, HIGH);
        }
        else if (current_scd41_co2 <= co2VeryHigh && current_scd41_co2 > co2High)
        {
            digitalWrite(RGB_LED_RED_PIN, LOW);
            digitalWrite(RGB_LED_GREEN_PIN, HIGH);
            digitalWrite(RGB_LED_BLUE_PIN, HIGH);
        }
        else if (current_scd41_co2 <= co2High && current_scd41_co2 > co2Mid)
        {
            digitalWrite(RGB_LED_RED_PIN, LOW);
            digitalWrite(RGB_LED_GREEN_PIN, LOW);
            digitalWrite(RGB_LED_BLUE_PIN, HIGH);
        }
        else
        {
            digitalWrite(RGB_LED_RED_PIN, HIGH);
            digitalWrite(RGB_LED_GREEN_PIN, LOW);
            digitalWrite(RGB_LED_BLUE_PIN, HIGH);
        }

        Serial.println("Current values:");
        Serial.print("CO2: ");
        Serial.println(current_scd41_co2);
        Serial.print("Temperature: ");
        Serial.println(current_sht4x_temp_float);
        Serial.print("Humidity: ");
        Serial.println(current_sht4x_humid_float);

        doc["device_id"] = macStr;
        doc["co2"] = current_scd41_co2;
        doc["temperature"] = current_sht4x_temp_float;
        doc["humidity"] = current_sht4x_humid_float;

        char mqtt_message[128];
        serializeJson(doc, mqtt_message);

        if (millis() - loopStartTime >= loopWaitTime)
        {
            client.publish(topic, mqtt_message, true);
            loopWaitTime = 5 * 60 * 1000;
            loopStartTime = millis();
        }

        client.loop();

        break;

    case State::Calibration_Start:
        digitalWrite(RGB_LED_GREEN_PIN, HIGH);
        pin_blink(RGB_LED_BLUE_PIN, RGB_LED_RED_PIN, 1000, HIGH);

        client.publish(topic, "calibrating", true);

        scd4x.startPeriodicMeasurement();

        current_state = State::Calibration_Wait_5_Min;
        currentWaitTime = 5 * 60 * 1000;
        stepStartTime = millis();
        break;

    case State::Calibration_Wait_5_Min:
        if (millis() - stepStartTime >= currentWaitTime)
        {
            scd4x.stopPeriodicMeasurement();
            current_state = State::Calibration_Wait_500_Ms;
            currentWaitTime = 5 * 1000;
            stepStartTime = millis();
        }
        pin_blink(RGB_LED_BLUE_PIN, RGB_LED_RED_PIN, 1000, HIGH);
        break;

    case State::Calibration_Wait_500_Ms:
        if (millis() - stepStartTime >= currentWaitTime)
        {
            scd4x.performForcedRecalibration(co2CalibValue, frcCorrection);
            Serial.print(frcCorrection);
            scd4x.startPeriodicMeasurement();
            current_state = State::Operation;
        }
        pin_blink(RGB_LED_BLUE_PIN, RGB_LED_RED_PIN, 1000, HIGH);
        break;
    }
}

void setup_wifi()
{
    Serial.print("Connecting to ");
    Serial.println(ssid);

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, LOW);

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

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, HIGH);

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(macStr);
}

void setup_ota()
{
    Serial.print("Setting up OTA Update functionality");

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, LOW);

    ArduinoOTA.onStart([]()
    {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
            type = "sketch";
        }
        else
        {
            type = "filesystem";
        }

        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]()
    {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
        {
            Serial.println("Auth Failed");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            Serial.println("Begin Failed");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            Serial.println("Connect Failed");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            Serial.println("Receive Failed");
        }
        else if (error == OTA_END_ERROR)
        {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.begin();

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, HIGH);

    Serial.println("");
    Serial.print("OTA setup completed");
    Serial.println("");
}

void setup_mqtt()
{
    Serial.print("Connecting to MQTT Server at ");
    Serial.println(mqtt_server);

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, LOW);

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    while (!client.connected())
    {
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str()))
        {
            Serial.println("MQTT connected");

            char topic[50];
            strcpy(topic, "co2melder/command/#");
            client.subscribe(topic);

            Serial.print("Subscribed to ");
            Serial.println(topic);
        }
        else
        {
            delay(500);
            Serial.print(".");
        }
    }

    digitalWrite(RGB_LED_RED_PIN, HIGH);
    digitalWrite(RGB_LED_GREEN_PIN, HIGH);
    digitalWrite(RGB_LED_BLUE_PIN, HIGH);
}

void callback(char* topic, byte* payload, unsigned int length)
{
    DynamicJsonDocument doc(1024);
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    deserializeJson(doc, payload, length);

    if (!strstr(topic, "all") && !strstr(topic, macStr))
        return;

    if (doc["calibration_value"])
    {
        co2CalibValue = doc["calibration_value"];
        Serial.print("Changed CO2 calibration value to ");
        Serial.println(doc["calibration_value"] + "");
    }

    if (doc["calibration"])
    {
        current_state = State::Calibration_Start;
        Serial.println("Changed state to calibrating");
    }

    if (doc["co2_very_high"])
    {
        co2VeryHigh = doc["co2_very_high"];
        Serial.print("Changed CO2 very high value to ");
        Serial.println(doc["co2_very_high"] + "");
    }

    if (doc["co2_high"])
    {
        co2High = doc["co2_high"];
        Serial.print("Changed CO2 high value to ");
        Serial.println(doc["co2_high"] + "");
    }

    if (doc["co2_mid"])
    {
        co2Mid = doc["co2_mid"];
        Serial.print("Changed CO2 mid value to ");
        Serial.println(doc["co2_mid"] + "");
    }
}

void pin_blink(const int pin, const long interval, const int pull_resistor)
{
    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;
    if (previousMillis == 0)
    {
        digitalWrite(pin, pull_resistor);
    }

    if (currentMillis - previousMillis >= interval)
    {
        digitalWrite(pin, !digitalRead(pin));
        previousMillis = currentMillis;
    }
}

void pin_blink(const int pin1, const int pin2, const long interval, const int pull_resistor)
{
    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;
    if (previousMillis == 0)
    {
        digitalWrite(pin1, pull_resistor);
        digitalWrite(pin2, pull_resistor);
    }

    if (currentMillis - previousMillis >= interval)
    {
        digitalWrite(pin1, !digitalRead(pin1));
        digitalWrite(pin2, !digitalRead(pin2));
        previousMillis = currentMillis;
    }
}

void pin_blink_diff_times(const int pin, const long interval_wait, const long interval_on, const int pull_resistor)
{
    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;

    if (previousMillis == 0)
    {
        digitalWrite(pin, pull_resistor);
    }

    if (digitalRead(pin) == LOW)
    {
        if (currentMillis - previousMillis >= interval_on)
        {
            digitalWrite(pin, !digitalRead(pin));
            previousMillis = currentMillis;
        }
    }
    else
    {
        if (currentMillis - previousMillis >= interval_wait)
        {
            digitalWrite(pin, !digitalRead(pin));
            previousMillis = currentMillis;
        }
    }
}
