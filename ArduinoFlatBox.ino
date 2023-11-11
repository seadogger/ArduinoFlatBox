/*
  What: ArduinoFlatBox - PC controlled lightbox implmented using the
  Alnitak (Flip-Flat/Flat-Man) command set found here:
  https://www.optecinc.com/astronomy/catalog/alnitak/resources/Alnitak_GenericCommandsR4.pdf

  Who:
  Created By: Jared Wellman - jared@mainsequencesoftware.com
  Adapted to V4 protocol, motor handling added By: Igor von Nyssen - igor@vonnyssen.com
  Modified to work with servo motors by Marco Cipriani, (GitHub @marcocipriani01)
  Modified to work with LDX-227 servo motor,kstars,EKOS by Seadogger, (Github @seadogger)

  When:
  Last modified:  5/8/2022

  Typical usage on the command prompt:
  Send     : >SOOO\r      //request state
  Recieve  : *S19OOO\n    //returned state

  Send     : >B128\r      //set brightness 128
  Recieve  : *B19128\n    //confirming brightness set to 128

  Send     : >JOOO\r      //get brightness
  Recieve  : *B19128\n    //brightness value of 128 (assuming as set from above)

  Send     : >LOOO\r      //turn light on (uses set brightness value)
  Recieve  : *L19OOO\n    //confirms light turned on

  Send     : >DOOO\r      //turn light off (brightness value should not be changed)
  Recieve  : *D19OOO\n    //confirms light turned off.

  Tested with an Arduino Nano and the NINA imaging software (https://nighttime-imaging.eu)
  Tested with an Arduino Nano and the kStars and StellarMate imaging software with Alnitak Flip Flat INDI Driver 
*/

#define LED_PIN 3
#define SERVO_PIN 11
#define CLOSED_ANGLE 270
#define OPEN_ANGLE 180
#define OPEN_PWM 1675
#define CLOSED_PWM 2400
#define SERVO_DELAY 25
#define STARTUP_DELAY 2000
//#define MINVOLTAGE 110
//#define MAXVOLTAGE 255

#include <Servo.h>

Servo servo;

enum devices {
  FLAT_MAN_L = 10,
  FLAT_MAN_XL = 15,
  FLAT_MAN = 19,
  FLIP_FLAT = 99
};

enum motorStatuses {
  STOPPED = 0,
  RUNNING
};

enum lightStatuses {
  OFF = 0,
  ON
};

enum shutterStatuses {
  NEITHER_OPEN_NOR_CLOSED = 0, // ie not open or closed...could be moving
  CLOSED,
  OPEN,
  TIMED_OUT
};

enum motorDirection {
  OPENING = 0,
  CLOSING,
  NONE
};

int deviceId = FLIP_FLAT; //set this to FLAT_MAN if you want to remove or not use the motor handling
int motorStatus = STOPPED;
int coverStatus = CLOSED;
int motorDirection = NONE;
int targetAngle = CLOSED_ANGLE;
int currentAngle = CLOSED_ANGLE;
int lightStatus = OFF;
int brightness = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, brightness);
  servo.attach(SERVO_PIN, OPEN_PWM, CLOSED_PWM);
  servo.writeMicroseconds(CLOSED_PWM);
  delay(STARTUP_DELAY);
}

void loop() {
  handleSerial();
  handleMotor();
}

void handleSerial() {
  if (Serial.available() >= 6) { // all incoming communications are fixed length at 6 bytes including the \n
    char* cmd;
    char* data;
    char temp[10];

    char str[20];
    memset(str, 0, 20);
    Serial.readBytesUntil('\r', str, 20);

    cmd = str + 1;
    data = str + 2;

    switch (*cmd) {
      /*
        Ping device
        Request: >POOO\r
        Return : *PiiOOO\n
        id = deviceId
      */
      case 'P':
        sprintf(temp, "*P%dOOO\n", deviceId);
        Serial.write(temp);
        break;

      /*
        Open shutter
        Request: >OOOO\r
        Return : *OiiOOO\n
        id = deviceId

        This command is only supported on the Flip-Flat, set the deviceId accordingly
      */
      case 'O':
        sprintf(temp, "*O%dOOO\n", deviceId);
        setShutter(OPEN);
        Serial.write(temp);
        analogWrite(LED_PIN, 0);
        break;

      /*
        Close shutter
        Request: >COOO\r
        Return : *CiiOOO\n
        id = deviceId

        This command is only supported on the Flip-Flat, set the deviceId accordingly
      */
      case 'C':
        sprintf(temp, "*C%dOOO\n", deviceId);
        setShutter(CLOSED);
        Serial.write(temp);
        break;

      /*
        Turn light on
        Request: >LOOO\r
        Return : *LiiOOO\n
        id = deviceId
      */
      case 'L':
        sprintf(temp, "*L%dOOO\n", deviceId);
        Serial.write(temp);
        lightStatus = ON;
        if (coverStatus == CLOSED) {
          analogWrite(LED_PIN, brightness);
        }
        break;

      /*
        Turn light off
        Request: >DOOO\r
        Return : *DiiOOO\n
        id = deviceId
      */
      case 'D':
        sprintf(temp, "*D%dOOO\n", deviceId);
        Serial.write(temp);
        lightStatus = OFF;
        analogWrite(LED_PIN, 0);
        break;

      /*
        Set brightness
        Request: >Bxxx\r
         xxx = brightness value from 000-255
        Return : *Biiyyy\n
        id = deviceId
         yyy = value that brightness was set from 000-255
      */
      case 'B': {
        
//        EL Panel turns off at about 110 so we cannot go down to 0 so remap the input to this range before converting
//        int tempdata = map(atoi(data), 0, 255, MINVOLTAGE, MAXVOLTAGE);
        
// Seadogger 5/22/2022 - removed conversion to make brightness linear.  Just want the panel to reflect what we put on the INDI GUI
//        double db = (double) (tempdata - 3);
//        brightness = constrain((int) (0.00403 * db * db), 0, 255);
        brightness = constrain(atoi(data), 0, 255);
        
        if ((lightStatus == ON) && (coverStatus == CLOSED)) {
          analogWrite(LED_PIN, brightness);
        }
        sprintf(temp, "*B%d%03d\n", deviceId, brightness);
        Serial.write(temp);
        break;
      }

      /*
        Get brightness
        Request: >JOOO\r
        Return : *Jiiyyy\n
        id = deviceId
         yyy = current brightness value from 000-255
      */
      case 'J':
        sprintf(temp, "*J%d%03d\n", deviceId, brightness);
        Serial.write(temp);
        break;

      /*
        Get device status:
        Request: >SOOO\r
        Return : *SidMLC\n
        id = deviceId
         M  = motor status( 0 stopped, 1 running)
         L  = light status( 0 off, 1 on)
         C  = Cover Status( 0 moving, 1 closed, 2 open, 3 timed out)
      */
      case 'S':
        sprintf(temp, "*S%d%d%d%d\n", deviceId, motorStatus, lightStatus, coverStatus);
        Serial.write(temp);
        break;

      /*
        Get firmware version
        Request: >VOOO\r
        Return : *Vii001\n
        id = deviceId
      */
      case 'V': // get firmware version
        sprintf(temp, "*V%d003\n", deviceId);
        Serial.write(temp);
        break;
    }
  }
}

void setShutter(int val) {
  if (val == OPEN && coverStatus != OPEN) {
    motorDirection = OPENING;
    targetAngle = OPEN_ANGLE;
  } else if (val == CLOSED && coverStatus != CLOSED) {
    motorDirection = CLOSING;
    targetAngle = CLOSED_ANGLE;
  }
}

void handleMotor() {
  if ((currentAngle > targetAngle) && (motorDirection == OPENING)) {
    motorStatus = RUNNING;
    coverStatus = NEITHER_OPEN_NOR_CLOSED;
    servo.writeMicroseconds(OPEN_PWM);
    --currentAngle;
    if (currentAngle <= targetAngle) {
      motorStatus = STOPPED;
      motorDirection = NONE;
      coverStatus = OPEN;
    }
    delay(SERVO_DELAY);

  } else if ((currentAngle < targetAngle) && (motorDirection == CLOSING)) {
    motorStatus = RUNNING;
    coverStatus = NEITHER_OPEN_NOR_CLOSED;
    servo.writeMicroseconds(CLOSED_PWM);
    ++currentAngle;
    if (currentAngle >= targetAngle) {
      motorStatus = STOPPED;
      motorDirection = NONE;
      coverStatus = CLOSED;
      if (lightStatus == ON) {
        analogWrite(LED_PIN, brightness);
      }
    }
    delay(SERVO_DELAY);
  }
}
