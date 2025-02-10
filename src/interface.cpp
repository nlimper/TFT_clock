#include "interface.h"
#include "config.h"
#include "menutree.h"
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>

Adafruit_LIS3DH lis = Adafruit_LIS3DH();

uint8_t currOrientation = 0;
extern bool flipOrientation;
extern void alarmAck();

volatile int lastEncoded = 0;
volatile long encoderValue = 0;
volatile long lastencoderValue = 0;
volatile int direction = 0;
volatile unsigned long lastEncoderTime = 0;
volatile int stepSize = 0;
volatile unsigned long lastDebounceTime = 0;
volatile int sum = 0;

void IRAM_ATTR updateEncoder() {
    int MSB = digitalRead(buttons[0].pin);
    int LSB = digitalRead(buttons[1].pin);

    int encoded = (MSB << 1) | LSB;
    sum = (lastEncoded << 2) | encoded;

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;

    lastEncoded = encoded;

    if (encoderValue != lastencoderValue) {
        if (encoderValue > lastencoderValue) {
            direction = -1;
        } else {
            direction = 1;
        }
        lastencoderValue = encoderValue;
    }
}

void initInterface() {

    for (int i = 0; i < buttonCount; i++) {
        buttons[i].lastState = HIGH;
        buttons[i].pressed = false;
        buttons[i].lastDebounceTime = 0;
        buttons[i].pressStartTime = 0;
        buttons[i].lastRepeatTime = 0;
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    if (hardware.rotary) {
        attachInterrupt(digitalPinToInterrupt(buttons[0].pin), updateEncoder, CHANGE);
        attachInterrupt(digitalPinToInterrupt(buttons[1].pin), updateEncoder, CHANGE);
    }
}

void onButtonPress(int buttonIndex) {
    alarmAck();
    handleMenuInput(buttonIndex);
}

void onButtonPress(int buttonIndex, int stepSize) {
    alarmAck();
    handleMenuInput(buttonIndex, stepSize);
}

void onButtonHold(int buttonIndex) {
    handleMenuHold(buttonIndex);
}

void rotary_loop() {
    // https://www.best-microcontroller-projects.com/rotary-encoder.html
    if (direction != 0) {
        int encstate = (digitalRead(buttons[0].pin) << 1) | digitalRead(buttons[1].pin);
        if (encstate == 0 || encstate == 3) {
            unsigned long currentTime = millis();
            unsigned long timeDiff = currentTime - lastEncoderTime;
            lastEncoderTime = currentTime;
            if (timeDiff < 80) {
                stepSize = 6;
            } else if (timeDiff < 150) {
                stepSize = 2;
            } else {
                stepSize = 1;
            }
            if (direction == 1) {
                onButtonPress(1, stepSize);
            } else if (direction == -1) {
                onButtonPress(0, stepSize);
            }
            direction = 0;
        }
    }
}

void readButton(uint8_t button) {
    int reading = digitalRead(buttons[button].pin);

    if (reading == LOW) {
        if (!buttons[button].pressed) {
            // First button press
            Serial.println("button " + String(button) + " pressed");
            onButtonPress(button);
            buttons[button].pressed = true;
            buttons[button].pressStartTime = millis();
            buttons[button].lastRepeatTime = buttons[button].pressStartTime;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            // Button is being held
            unsigned long holdDuration = millis() - buttons[button].pressStartTime;
            if (holdDuration > 500) {
                // Calculate the repeat interval: start at 250ms, decrease to 5ms
                unsigned long repeatInterval = (holdDuration / 10 > 300) ? 5 : 300 - (holdDuration / 10);
                if (millis() - buttons[button].lastRepeatTime >= repeatInterval) {
                    Serial.println("button " + String(button) + " hold");
                    onButtonHold(button);
                    buttons[button].lastRepeatTime = millis();
                }
            }
        }
    } else if (reading == HIGH) {
        buttons[button].pressed = false;
        buttons[button].pressStartTime = 0;
        buttons[button].lastRepeatTime = 0;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void interfaceRun() {
    if (hardware.rotary) {
        rotary_loop();
        readButton(2);
    } else {
        for (int i = 0; i < buttonCount; i++) {
            readButton(i);
        }
    }
}

void initAccelerometer() {
    if (!lis.begin(0x19)) {
        Serial.println("Couldnt start lis3dh");
    } else {
        Serial.println("LIS3DH found!");
        lis.setRange(LIS3DH_RANGE_4_G); // 2, 4, 8 or 16 G!
        // 0 = turn off click detection & interrupt
        // 1 = single click only interrupt output
        // 2 = double click only interrupt output, detect single click
        // Adjust threshhold, higher numbers are less sensitive
        lis.setClick(2, 100, 50, 30, 255);
        hasAccelerometer = true;
    }
}
void accelerometerRun() {
    if (!hasAccelerometer) return;

    uint8_t click = lis.getClick();
    if (click & 0x30) {
        Serial.print("Click detected (0x");
        Serial.print(click, HEX);
        Serial.print("): ");
        if (click & 0x10) Serial.print(" single click");
        if (click & 0x20) {
            Serial.print(" double click");
            alarmAck();
        }
        Serial.println();
    }

    sensors_event_t event;
    lis.getEvent(&event);
    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;

    uint8_t side = 0;
    if (abs(x) > abs(y) && abs(x) > abs(z)) {
        side = (x > 0) ? 1 : 2; // 1: Right side up, 2: Left side up
    } else if (abs(y) > abs(x) && abs(y) > abs(z)) {
        side = (y > 0) ? 3 : 4; // 3: Front side up, 4: Back side up
    } else {
        side = (z > 0) ? 5 : 6; // 5: Bottom side up, 6: Top side up
    }

    if (side != currOrientation) {
        alarmAck();
        currOrientation = side;
        Serial.print("Orientation: ");
        Serial.println(side);
        if (String(config.fliporientation).indexOf(String(currOrientation)) > -1) {
            flipOrientation = true;
            d1 = 10;
            d2 = 10;
            d3 = 10;
            prevMinute = -1;
        } else if (flipOrientation == true) {
            flipOrientation = false;
            d1 = 10;
            d2 = 10;
            d3 = 10;
            prevMinute = -1;
        }
    }
}
