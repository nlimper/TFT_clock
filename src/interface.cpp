#include "interface.h"
#include "config.h"
#include "menutree.h"
#ifndef DISABLE_ACCELEROMETER
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#endif
#include <Arduino.h>

#ifndef DISABLE_ACCELEROMETER
Adafruit_LIS3DH lis = Adafruit_LIS3DH();
#endif

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

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue = encoderValue + 1;
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue = encoderValue - 1;

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

#ifndef DISABLE_ACCELEROMETER
const float alpha = 0.2;
float filteredX = 0, filteredY = 0, filteredZ = 0;

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

    sensors_event_t event;
    lis.getEvent(&event);
    filteredX = event.acceleration.x;
    filteredY = event.acceleration.y;
    filteredZ = event.acceleration.z;
}

uint8_t accelerometerRun(bool active) {
    if (!hasAccelerometer) return 0;
    
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
    filteredX = alpha * event.acceleration.x + (1 - alpha) * filteredX;
    filteredY = alpha * event.acceleration.y + (1 - alpha) * filteredY;
    filteredZ = alpha * event.acceleration.z + (1 - alpha) * filteredZ;

    uint8_t side = 0;
    if (abs(filteredX) > abs(filteredY) && abs(filteredX) > abs(filteredZ)) {
        side = (filteredX > 0) ? 1 : 2;
    } else if (abs(filteredY) > abs(filteredX) && abs(filteredY) > abs(filteredZ)) {
        side = (filteredY > 0) ? 3 : 4;
    } else {
        side = (filteredZ > 0) ? 5 : 6;
    }

    if (active && side != currOrientation) {
        alarmAck();
        currOrientation = side;
        Serial.print("Orientation: ");
        Serial.println(side);
        bool newFlip = (String(config.fliporientation).indexOf(String(currOrientation)) > -1);
        if (newFlip != flipOrientation) {
            flipOrientation = newFlip;
            d1 = 10;
            d2 = 10;
            d3 = 10;
            prevMinute = -1;
        }
    }

    return side;
}
#else
// Stub implementations when accelerometer is disabled
void initAccelerometer() {
    Serial.println("Accelerometer disabled");
    hasAccelerometer = false;
}

uint8_t accelerometerRun(bool active) {
    return 0;
}
#endif
