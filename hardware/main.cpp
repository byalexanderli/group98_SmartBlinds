#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <ESP32Servo.h>
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

const char* WIFI_SSID = "ilovedimsum";
const char* WIFI_PASSWORD = "goodbun!";

const char* AWS_SERVER = "18.191.225.179";
const int AWS_PORT = 5000;

const int SERVO_PIN = 13;
const int LIGHT_SENSOR_PIN = 36;

const float BLINDS_MAX_LENGTH = 100.0;
const float MIN_OPENING = 0.0;
const float MAX_OPENING = 100.0;

const int FULLY_OPEN = 180;
const int FULLY_CLOSED = 0;

const int LIGHT_TARGET_LOW = 300;
const int LIGHT_TARGET_HIGH = 500;

Servo blindsServo;
Adafruit_AHTX0 aht;
char ssid[50];
char pass[50];

int currentAngle = 0;
float currentOpening = 0.0;
bool automaticModeEnabled = false;
bool isServoBusy = false;
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_INTERVAL = 1000;

int analogToLux(int analogValue) {
    return map(analogValue, 0, 4095, 0, 1000);
}

float calculateOpeningLength(int angle) {
    return (float)angle * (MAX_OPENING / FULLY_OPEN);
}

void nvs_access() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    Serial.printf("\nOpening NVS handle");
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    
    if (err != ESP_OK) {
        Serial.printf("Error (%s) opening NVS handle\n", esp_err_to_name(err));
    } else {
        size_t ssid_len;
        size_t pass_len;
        err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
        err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
        
        switch (err) {
            case ESP_OK:
                Serial.printf("Done\n");
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                Serial.printf("Value isnt initialized yet\n");
                break;
            default:
                Serial.printf("Error (%s) reading\n", esp_err_to_name(err));
        }
        nvs_close(my_handle);
    }
}

void setBlindsPosition(int angle, bool isManual) {  
    if (!blindsServo.attached()) {
        ESP32PWM::allocateTimer(0);
        blindsServo.setPeriodHertz(50);
        blindsServo.attach(SERVO_PIN, 500, 2500);
        delay(100);
    }

    angle = constrain(angle, FULLY_CLOSED, FULLY_OPEN);
    
    Serial.println("Writing angle to servo...");
    blindsServo.write(angle);
    delay(500);
    
    currentAngle = angle;
    currentOpening = calculateOpeningLength(angle);
}

void automaticControl(int lightLevel) {
    if (!automaticModeEnabled) {
        return;
    }

    int luxLevel = analogToLux(lightLevel);
    if (luxLevel < LIGHT_TARGET_LOW || luxLevel > LIGHT_TARGET_HIGH) {
        int targetAngle = map(luxLevel, LIGHT_TARGET_LOW, LIGHT_TARGET_HIGH, FULLY_OPEN, FULLY_CLOSED);
        targetAngle = constrain(targetAngle, 30, 150);
        setBlindsPosition(targetAngle, false);
    }
}

void sendSensorData(float temperature, float humidity, int lightLevel, int angle, float opening) {
    WiFiClient client;
    if (client.connect(AWS_SERVER, AWS_PORT)) {
        String url = String("GET /?") +
                    "temp=" + String(temperature, 2) +
                    "&hum=" + String(humidity, 2) +
                    "&light=" + String(analogToLux(lightLevel)) +
                    "&angle=" + String(angle) +
                    "&opening=" + String(opening, 2) +
                    " HTTP/1.1";

        client.println(url);
        client.print("Host: ");
        client.println(AWS_SERVER);
        client.println();

        unsigned long timeout = millis() + 10000;
        while (client.connected() && millis() < timeout) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                if (line == "\r") {
                    break;
                }
            }
        }

        if (millis() >= timeout) {
            Serial.println("Timeout waiting for response");
        } else {
            Serial.println("Response received from AWS EC2 instance");
        }

        while (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println(line);
        }

        client.stop();
    } else {
        Serial.println("Connection to AWS EC2 instance failed");
    }
}

void checkCommands() {
    
    WiFiClient client;
    if (client.connect(AWS_SERVER, AWS_PORT)) {
        Serial.println("Connected to server");
        
        // Request
        String request = "GET /control HTTP/1.1\r\n";
        request += "Host: " + String(AWS_SERVER) + "\r\n";
        request += "Connection: close\r\n\r\n";
        
        client.print(request);

        unsigned long timeout = millis();
        String response = "";
        
        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                client.stop();
                return;
            }
        }

        while (client.available()) {
            String line = client.readStringUntil('\n');
            response += line + "\n";

            if (line.indexOf("\"command\"") != -1) {
                if (line.indexOf("\"open\"") != -1) {
                    blindsServo.write(FULLY_OPEN);
                    delay(1000);
                    currentAngle = FULLY_OPEN;
                }
                else if (line.indexOf("\"close\"") != -1) {
                    blindsServo.write(FULLY_CLOSED);
                    delay(1000);
                    currentAngle = FULLY_CLOSED;
                }
            }
        }
        
     
        client.stop();
    } else {
        Serial.println("Failed to connect to server");
    }
}

void setup() {
    Serial.begin(9600);
    delay(1000);
    Wire.begin();
    if (!aht.begin()) {
        Serial.println("Could not find AHT sensor");
        while (1) delay(10);
    }

    nvs_access();
    Serial.println();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    ESP32PWM::allocateTimer(0);
    blindsServo.setPeriodHertz(50);
    blindsServo.attach(SERVO_PIN, 500, 2500);


    ESP32PWM::allocateTimer(0);
    blindsServo.setPeriodHertz(50);
    blindsServo.write(0);
    delay(1000);
    blindsServo.write(180);
    delay(1000);
    blindsServo.write(90);
}

void loop() {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    int lightLevel = analogRead(LIGHT_SENSOR_PIN);
    
    if (!isnan(temp.temperature) && !isnan(humidity.relative_humidity)) {
        sendSensorData(temp.temperature, humidity.relative_humidity, lightLevel, currentAngle, currentOpening);

        checkCommands();

        if (automaticModeEnabled) {
            Serial.println("Auto mode enabled - checking light levels");
            automaticControl(lightLevel);
        } else {
            Serial.println("Auto mode disabled - manual control only");
        }
    }
    
    delay(5000);
}