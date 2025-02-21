#include <Wire.h>
#include "AccelStepper.h"

// Select board type
#define BOARD_CNC_SHIELD_V3_0
// #define BOARD_HW_702

#ifdef BOARD_CNC_SHIELD_V3_0
  #include "cnc_shield_v3_0.h"
#endif

#ifdef BOARD_HW_702
  #include "hw_702.h"
#endif

#define MAJOR_VERSION 1
#define MINOR_VERSION 1
#define PATCH_VERSION 1

#define TARGET_POS_TYPE_SET 0
#define TARGET_POS_TYPE_ADD 1

#define MODE_STOP 0
#define MODE_RUN_CONTINUOUS 1
#define MODE_RUN_TO_POS 30
#define MODE_RUN_TO_POS_W_RAMP 31

#define I2C_ADDRESS 0x55

#define VERSION_REGISTER 0x00
#define RESET_REGISTER 0x01
#define ENABLE_REGISTER 0x02
#define STOP_REGISTER 0x20
#define RUN_CONTINUOUS_REGISTER 0x30
#define RUN_TO_POS_REGISTER 0x40
#define RUN_TO_POS_W_RAMP_REGISTER 0x50
#define POSITION_REGISTER 0x60
#define SPEED_REGISTER 0x70
#define ACCELERATION_REGISTER 0x80
#define RUNNING_REGISTER 0x90

const byte VERSION[] = { MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION };

volatile uint8_t registerPtr = 0;

AccelStepper steppers[4] = {
    AccelStepper(AccelStepper::DRIVER, X_STEP, X_DIR),
    AccelStepper(AccelStepper::DRIVER, Y_STEP, Y_DIR),
    AccelStepper(AccelStepper::DRIVER, Z_STEP, Z_DIR),
    AccelStepper(AccelStepper::DRIVER, A_STEP, A_DIR)
};
volatile uint8_t mode[4];

void i2cRxHandler(int numBytes) {
  registerPtr = Wire.read(); // First byte always sets ptr

  // Reset
  if (registerPtr == RESET_REGISTER && numBytes == 2) {
    if (Wire.read() == 0x1) {
      resetSteppers();
    }

  // Enable
  } else if (registerPtr == ENABLE_REGISTER && numBytes == 2) {
    if (Wire.read() == 0x1) {
      ENABLE_PORT &= ~ENABLE_SET;
    } else {
      ENABLE_PORT |= ENABLE_SET;
    }

  // Stop
  } else if (registerPtr >= STOP_REGISTER && registerPtr < STOP_REGISTER + 4 && numBytes == 2) {
    uint8_t index = registerPtr - STOP_REGISTER;
    if (Wire.read() == 0x1) {
      mode[index] = MODE_STOP;
      steppers[index].move(0);
      steppers[index].setSpeed(0);
    }

  // Run continuous
  } else if (registerPtr >= RUN_CONTINUOUS_REGISTER && registerPtr < RUN_CONTINUOUS_REGISTER + 4 && numBytes == 5) {
    uint8_t index = registerPtr - RUN_CONTINUOUS_REGISTER;
    float speed;

    byte *bytes = (byte*)&speed;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();
    steppers[index].setSpeed(speed);
    mode[index] = MODE_RUN_CONTINUOUS;

  // Run to target position without ramp
  } else if (registerPtr >= RUN_TO_POS_REGISTER && registerPtr < RUN_TO_POS_REGISTER + 4 && numBytes == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_REGISTER;
    float speed;
    uint8_t rel;
    int32_t position;

    byte *bytes = (byte*)&speed;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    rel = Wire.read();

    bytes = (byte*)&position;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    steppers[index].setMaxSpeed(speed);
    if (rel) {
      steppers[index].move(position);
    } else {
      steppers[index].moveTo(position);
    }
    steppers[index].setSpeed(speed);
    mode[index] = MODE_RUN_TO_POS;

  // Run to target position with ramp
  } else if (registerPtr >= RUN_TO_POS_W_RAMP_REGISTER && registerPtr < RUN_TO_POS_W_RAMP_REGISTER + 4 && numBytes == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_W_RAMP_REGISTER;
    float speed;
    uint8_t rel;
    int32_t position;

    byte *bytes = (byte*)&speed;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    rel = Wire.read();

    bytes = (byte*)&position;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    if (rel) {
      steppers[index].move(position);
    } else {
      steppers[index].moveTo(position);
    }
    steppers[index].setMaxSpeed(speed);
    mode[index] = MODE_RUN_TO_POS_W_RAMP;

  // Set position
  } else if (registerPtr >= POSITION_REGISTER && registerPtr < POSITION_REGISTER + 4 && numBytes == 5) {
    uint8_t index = registerPtr - POSITION_REGISTER;
    int32_t position;

    byte *bytes = (byte*)&position;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    steppers[index].setCurrentPosition(position);

  // Set acceleration
  } else if (registerPtr >= ACCELERATION_REGISTER && registerPtr < ACCELERATION_REGISTER + 4 && numBytes == 5) {
    uint8_t index = registerPtr - ACCELERATION_REGISTER;
    float acceleration;

    byte *bytes = (byte*)&acceleration;
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    steppers[index].setAcceleration(acceleration);
  }

}

void i2cReqHandler(void) {
  // Version
  if (registerPtr == VERSION_REGISTER) {
    Wire.write(VERSION, 3);

  // Position
  } else if (registerPtr >= POSITION_REGISTER && registerPtr < POSITION_REGISTER + 4) {
    uint8_t index = registerPtr - POSITION_REGISTER;
    int32_t position = steppers[index].currentPosition();
    Wire.write((byte*)&position, 4);

  // Speed
  } else if (registerPtr >= SPEED_REGISTER && registerPtr < SPEED_REGISTER + 4) {
    uint8_t index = registerPtr - SPEED_REGISTER;
    float speed = steppers[index].speed();
    Wire.write((byte*)&speed, 4);

  // Running
  } else if (registerPtr >= RUNNING_REGISTER && registerPtr < RUNNING_REGISTER + 4) {
    uint8_t index = registerPtr - RUNNING_REGISTER;
    char running = steppers[index].isRunning();

    Wire.write(running);

  }
}

void initPins() {
  pinMode(8, OUTPUT); // Enable
  digitalWrite(9, 0); // LOW to enable
}

void resetSteppers() {
  initPins();
  for (char i=0; i<4; i++) {
    steppers[i].setCurrentPosition(0);
    steppers[i].moveTo(0);
    steppers[i].setSpeed(0);
    steppers[i].setAcceleration(1);
    steppers[i].setMaxSpeed(1);
    mode[i] = MODE_STOP;
  }
}

void initI2C() {
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(i2cRxHandler);
  Wire.onRequest(i2cReqHandler);
}

void setup() {
  initI2C();
  resetSteppers();

  Serial.begin(9600);
}

void loop() {
  for (int i=0; i<4; i++) {
    if (mode[i] == MODE_RUN_CONTINUOUS) {
      steppers[i].runSpeed();
    } else if (mode[i] == MODE_RUN_TO_POS) {
      steppers[i].runSpeedToPosition();
    } else if (mode[i] == MODE_RUN_TO_POS_W_RAMP) {
      steppers[i].run();
    }
  }
}
