#ifndef FLAT_BOX_H
#define FLAT_BOX_H

#include <Arduino.h>
#include <Servo.h>

#define LED_PIN 6
#define SWITCH_PIN 10
#define SERVO_PIN 9
#define CLOSED_ANGLE 270
#define OPEN_ANGLE 0
#define SERVO_DELAY 15
#define SERVO_INITIAL_WAIT 2000

void handleSerial();
void setShutter(int val);
void handleMotor();

#endif
