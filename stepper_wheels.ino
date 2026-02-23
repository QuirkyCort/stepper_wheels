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
#define PATCH_VERSION 4

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
volatile uint8_t buffer[10];
volatile uint8_t bufferLen = 0;
volatile bool preIsRunning[4] = {false, false, false, false};

AccelStepper steppers[4] = {
    AccelStepper(AccelStepper::DRIVER, X_STEP, X_DIR),
    AccelStepper(AccelStepper::DRIVER, Y_STEP, Y_DIR),
    AccelStepper(AccelStepper::DRIVER, Z_STEP, Z_DIR),
    AccelStepper(AccelStepper::DRIVER, A_STEP, A_DIR)
};
volatile uint8_t mode[4];

void i2cRxHandler(int numBytes) {
  bufferLen = numBytes;
  for (uint8_t i=0; i<numBytes; i++) {
    buffer[i] = Wire.read();
  }

  // Set preIsRunning to avoid race when a IsRunning request is received before the Rx is processed.
  registerPtr = buffer[0]; // First byte always sets ptr

  // Run continuous
  if (registerPtr >= RUN_CONTINUOUS_REGISTER && registerPtr < RUN_CONTINUOUS_REGISTER + 4 && bufferLen == 5) {
    uint8_t index = registerPtr - RUN_CONTINUOUS_REGISTER;
    preIsRunning[index] = true;


  // Run to target position without ramp
  } else if (registerPtr >= RUN_TO_POS_REGISTER && registerPtr < RUN_TO_POS_REGISTER + 4 && bufferLen == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_REGISTER;
    preIsRunning[index] = true;


  // Run to target position with ramp
  } else if (registerPtr >= RUN_TO_POS_W_RAMP_REGISTER && registerPtr < RUN_TO_POS_W_RAMP_REGISTER + 4 && bufferLen == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_W_RAMP_REGISTER;
    preIsRunning[index] = true;
  }
}

void processRx() {
  registerPtr = buffer[0]; // First byte always sets ptr

  if (bufferLen == 0) {
    return;
  }

  // Reset
  if (registerPtr == RESET_REGISTER && bufferLen == 2) {
    if (buffer[1] == 0x1) {
      resetSteppers();
      digitalWrite(ENABLE_PIN, 0);
    }

  // Enable
  } else if (registerPtr == ENABLE_REGISTER && bufferLen == 2) {
    if (buffer[1] == 0x1) {
      digitalWrite(ENABLE_PIN, 0);
    } else {
      digitalWrite(ENABLE_PIN, 1);
    }

  // Stop
  } else if (registerPtr >= STOP_REGISTER && registerPtr < STOP_REGISTER + 4 && bufferLen == 2) {
    uint8_t index = registerPtr - STOP_REGISTER;
    if (buffer[1] == 0x1) {
      mode[index] = MODE_STOP;
      steppers[index].move(0);
      steppers[index].setSpeed(0);
    }

  // Run continuous
  } else if (registerPtr >= RUN_CONTINUOUS_REGISTER && registerPtr < RUN_CONTINUOUS_REGISTER + 4 && bufferLen == 5) {
    uint8_t index = registerPtr - RUN_CONTINUOUS_REGISTER;
    float speed;

    byte *bytes = (byte*)&speed;
    bytes[0] = buffer[1];
    bytes[1] = buffer[2];
    bytes[2] = buffer[3];
    bytes[3] = buffer[4];
    steppers[index].setSpeed(speed);
    mode[index] = MODE_RUN_CONTINUOUS;
    preIsRunning[index] = false;

  // Run to target position without ramp
  } else if (registerPtr >= RUN_TO_POS_REGISTER && registerPtr < RUN_TO_POS_REGISTER + 4 && bufferLen == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_REGISTER;
    float speed;
    uint8_t rel;
    int32_t position;

    byte *bytes = (byte*)&speed;
    bytes[0] = buffer[1];
    bytes[1] = buffer[2];
    bytes[2] = buffer[3];
    bytes[3] = buffer[4];

    rel = buffer[5];

    bytes = (byte*)&position;
    bytes[0] = buffer[6];
    bytes[1] = buffer[7];
    bytes[2] = buffer[8];
    bytes[3] = buffer[9];

    steppers[index].setMaxSpeed(speed);
    if (rel) {
      steppers[index].move(position);
    } else {
      steppers[index].moveTo(position);
    }
    steppers[index].setSpeed(speed);
    mode[index] = MODE_RUN_TO_POS;
    preIsRunning[index] = false;

  // Run to target position with ramp
  } else if (registerPtr >= RUN_TO_POS_W_RAMP_REGISTER && registerPtr < RUN_TO_POS_W_RAMP_REGISTER + 4 && bufferLen == 10) {
    uint8_t index = registerPtr - RUN_TO_POS_W_RAMP_REGISTER;
    float speed;
    uint8_t rel;
    int32_t position;

    byte *bytes = (byte*)&speed;
    bytes[0] = buffer[1];
    bytes[1] = buffer[2];
    bytes[2] = buffer[3];
    bytes[3] = buffer[4];

    rel = buffer[5];

    bytes = (byte*)&position;
    bytes[0] = buffer[6];
    bytes[1] = buffer[7];
    bytes[2] = buffer[8];
    bytes[3] = buffer[9];

    if (rel) {
      steppers[index].move(position);
    } else {
      steppers[index].moveTo(position);
    }
    steppers[index].setMaxSpeed(speed);
    mode[index] = MODE_RUN_TO_POS_W_RAMP;
    preIsRunning[index] = false;

  // Set position
  } else if (registerPtr >= POSITION_REGISTER && registerPtr < POSITION_REGISTER + 4 && bufferLen == 5) {
    uint8_t index = registerPtr - POSITION_REGISTER;
    int32_t position;

    byte *bytes = (byte*)&position;
    bytes[0] = buffer[1];
    bytes[1] = buffer[2];
    bytes[2] = buffer[3];
    bytes[3] = buffer[4];

    steppers[index].setCurrentPosition(position);

  // Set acceleration
  } else if (registerPtr >= ACCELERATION_REGISTER && registerPtr < ACCELERATION_REGISTER + 4 && bufferLen == 5) {
    uint8_t index = registerPtr - ACCELERATION_REGISTER;
    float acceleration;

    byte *bytes = (byte*)&acceleration;
    bytes[0] = buffer[1];
    bytes[1] = buffer[2];
    bytes[2] = buffer[3];
    bytes[3] = buffer[4];

    steppers[index].setAcceleration(acceleration);
  }

  bufferLen = 0;
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
    if (preIsRunning[index]) {
      Wire.write(1);
    } else {
      char running = steppers[index].isRunning();
      Wire.write(running);
    }

  }
  
  bufferLen = 0;
}

void initPins() {
  pinMode(ENABLE_PIN, OUTPUT); // Enable
  digitalWrite(ENABLE_PIN, 0); // LOW to enable
}

void resetSteppers() {
  initPins();
  for (char i=0; i<4; i++) {
    steppers[i].setCurrentPosition(0);
    steppers[i].moveTo(0);
    steppers[i].setSpeed(0);
    steppers[i].setAcceleration(500);
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
  processRx();
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
