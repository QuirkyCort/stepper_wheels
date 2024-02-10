#include <Wire.h>

#define MAJOR_VERSION 1
#define MINOR_VERSION 1
#define PATCH_VERSION 1

#define I2C_ADDRESS 0x55

#define LOOP_PERIOD_MS 100
#define MIN_SPEED 10

#define TARGET_POS_TYPE_SET 0
#define TARGET_POS_TYPE_ADD 1

#define MODE_STOP 0
#define MODE_RUN_CONTINUOUS 1
#define MODE_RUN_TO_TARGET_TIME 20
#define MODE_RUN_TO_TARGET_TIME_W_RAMP 21
#define MODE_RUN_TO_TARGET_POS 30
#define MODE_RUN_TO_TARGET_POS_W_RAMP 31


const byte VERSION[] = { MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION };

volatile uint8_t registerPtr = 0;

volatile uint16_t counter[4];

volatile uint8_t enable;
volatile uint16_t trigger[4];
volatile uint8_t direction[4];
volatile uint8_t mode[4];
volatile int32_t position[4];
volatile int32_t targetPosition[4];
volatile uint16_t targetTime[4];

volatile uint16_t speed[4];
volatile uint16_t rampUpCounter[4];
volatile uint16_t rampUpDelta[4];
volatile int32_t cruiseEndPosition[4];
volatile uint16_t cruiseCounter[4];
volatile uint16_t cruiseSpeed[4];
volatile uint16_t rampDownCounter[4];
volatile uint16_t rampDownDelta[4];

unsigned long last_loop_time = 0;

void i2cRxHandler(int numBytes) {
  registerPtr = Wire.read(); // First byte always sets ptr

  // Reset
  if (registerPtr == 0x1 && numBytes == 2) {
    if (Wire.read() == 0x1) {
      resetSteppers();
    }

  // Enable
  } else if (registerPtr == 0x2 && numBytes == 2) {
    if (Wire.read() == 0x1) {
      PORTB &= B11111110;
      enable = 1;
    } else {
      PORTB |= B00000001;
      enable = 0;
    }

  // Trigger
  } else if (registerPtr >= 0x41 && registerPtr <= 0x44 && numBytes == 3) {
    byte *bytes = (byte*)&trigger[registerPtr - 0x41];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    counter[registerPtr - 0x41] = 0;

  // Direction
  } else if (registerPtr >= 0x45 && registerPtr <= 0x48 && numBytes == 2) {
    direction[registerPtr - 0x45] = Wire.read();
    if (registerPtr == 0x45) {
      if (direction[0] == 1) {
        PORTD |= B00100000;
      } else {
        PORTD &= B11011111;
      }
    } else if (registerPtr == 0x46) {
      if (direction[1] == 1) {
        PORTD |= B01000000;
      } else {
        PORTD &= B10111111;
      }
    } else if (registerPtr == 0x47) {
      if (direction[2] == 1) {
        PORTD |= B10000000;
      } else {
        PORTD &= B01111111;
      }
    } else if (registerPtr == 0x48) {
      if (direction[3] == 1) {
        PORTB |= B00100000;
      } else {
        PORTB &= B11011111;
      }
    }

  // Mode
  } else if (registerPtr >= 0x49 && registerPtr <= 0x4C && numBytes == 2) {
    uint8_t index = registerPtr - 0x49;
    mode[index] = Wire.read();

  // Position
  } else if (registerPtr >= 0x4D && registerPtr <= 0x50 && numBytes == 5) {
    byte *bytes = (byte*)&position[registerPtr - 0x4D];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

  // Target Position
  } else if (registerPtr >= 0x51 && registerPtr <= 0x54 && numBytes == 6) {
    uint8_t type = Wire.read();
    uint8_t index = registerPtr - 0x51;

    byte *bytes = (byte*)&targetPosition[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    if (type == TARGET_POS_TYPE_ADD) {
      targetPosition[index] += position[index];
    }

  // Target Time
  } else if (registerPtr >= 0x55 && registerPtr <= 0x58 && numBytes == 3) {
    byte *bytes = (byte*)&targetTime[registerPtr - 0x55];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

  // Target Steps with Ramp
  } else if (registerPtr >= 0x59 && registerPtr <= 0x5C && numBytes == 19) {
    uint8_t index = registerPtr - 0x59;

    byte *bytes = (byte*)&targetPosition[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    bytes = (byte*)&cruiseEndPosition[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

    if (direction[index] == 0) {
      targetPosition[index] += position[index];
      cruiseEndPosition[index] += position[index];
    } else {
      targetPosition[index] = position[index] - targetPosition[index];
      cruiseEndPosition[index] = position[index] - cruiseEndPosition[index];
    }

    bytes = (byte*)&rampUpCounter[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampUpDelta[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&cruiseSpeed[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampDownCounter[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampDownDelta[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    speed[index] = 0;

  // Target Time with Ramp
  } else if (registerPtr >= 0x5D && registerPtr <= 0x60 && numBytes == 13) {
    uint8_t index = registerPtr - 0x5D;

    byte *bytes = (byte*)&rampUpCounter[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampUpDelta[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&cruiseCounter[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&cruiseSpeed[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampDownCounter[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    bytes = (byte*)&rampDownDelta[index];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();

    speed[index] = 0;
  }

}

void i2cReqHandler(void) {
  // Version
  if (registerPtr == 0) {
    Wire.write(VERSION, 3);

  // Trigger
  } else if (registerPtr >= 0x41 && registerPtr <= 0x44) {
    Wire.write((byte*)&trigger[registerPtr - 0x41], 2);

  // Direction
  } else if (registerPtr >= 0x45 && registerPtr <= 0x48) {
    Wire.write(direction[registerPtr - 0x45]);

  // Mode
  } else if (registerPtr >= 0x49 && registerPtr <= 0x4C) {
    Wire.write(mode[registerPtr - 0x49]);

  // Position
  } else if (registerPtr >= 0x4D && registerPtr <= 0x50) {
    Wire.write((byte*)&position[registerPtr - 0x4D], 4);

  // Target Position
  } else if (registerPtr >= 0x51 && registerPtr <= 0x54) {
    Wire.write((byte*)&targetPosition[registerPtr - 0x51], 4);

  // Target Time
  } else if (registerPtr >= 0x55 && registerPtr <= 0x58) {
    Wire.write((byte*)&targetTime[registerPtr - 0x51], 2);

  }
}

// Loop unrolled to save clock cycles. Not sure if that's really needed.
ISR(TIMER2_OVF_vect) {
  if (trigger[0] && counter[0] == trigger[0]) {
    PORTD |= B00000100;
    counter[0] = 0;
    if (direction[0] == 0) {
      position[0]++;
    } else {
      position[0]--;
    }
    if (mode[0] >= MODE_RUN_TO_TARGET_POS) {
      if (direction[0] == 0) {
        if (position[0] >= targetPosition[0]) {
          trigger[0] = 0;
          mode[0] = MODE_STOP;
        }
      } else {
        if (position[0] <= targetPosition[0]) {
          trigger[0] = 0;
          mode[0] = MODE_STOP;
        }
      }
    }
  } else {
    PORTD &= B11111011;
  }

  if (trigger[1] && counter[1] == trigger[1]) {
    PORTD |= B00001000;
    counter[1] = 0;
    if (direction[1] == 0) {
      position[1]++;
    } else {
      position[1]--;
    }
    if (mode[1] >= MODE_RUN_TO_TARGET_POS) {
      if (direction[1] == 0) {
        if (position[1] >= targetPosition[1]) {
          trigger[1] = 0;
          mode[1] = MODE_STOP;
        }
      } else {
        if (position[1] <= targetPosition[1]) {
          trigger[1] = 0;
          mode[1] = MODE_STOP;
        }
      }
    }
  } else {
    PORTD &= B11110111;
  }

  if (trigger[2] && counter[2] == trigger[2]) {
    PORTD |= B00010000;
    counter[2] = 0;
    if (direction[2] == 0) {
      position[2]++;
    } else {
      position[2]--;
    }
    if (mode[2] >= MODE_RUN_TO_TARGET_POS) {
      if (direction[2] == 0) {
        if (position[2] >= targetPosition[2]) {
          trigger[2] = 0;
          mode[2] = MODE_STOP;
        }
      } else {
        if (position[2] <= targetPosition[2]) {
          trigger[2] = 0;
          mode[2] = MODE_STOP;
        }
      }
    }
  } else {
    PORTD &= B11101111;
  }

  if (trigger[3] && counter[3] == trigger[3]) {
    PORTB |= B00010000;
    counter[3] = 0;
    if (direction[3] == 0) {
      position[3]++;
    } else {
      position[3]--;
    }
    if (mode[3] >= MODE_RUN_TO_TARGET_POS) {
      if (direction[3] == 0) {
        if (position[3] >= targetPosition[3]) {
          trigger[3] = 0;
          mode[3] = MODE_STOP;
        }
      } else {
        if (position[3] <= targetPosition[3]) {
          trigger[3] = 0;
          mode[3] = MODE_STOP;
        }
      }
    }
  } else {
    PORTD &= B11101111;
  }

  counter[0]++;
  counter[1]++;
  counter[2]++;
  counter[3]++;
}

void setupTimer() {
  TCCR2A = 0;           // Init Timer2
  TCCR2B = B00000010;   // Prescaler = 8
  TIMSK2 = B00000001;   // Enable Timer Overflow Interrupt
}

void initPins() {
  pinMode(2, OUTPUT); // X step
  digitalWrite(2, 0);

  pinMode(5, OUTPUT); // X direction
  digitalWrite(5, 0);

  pinMode(3, OUTPUT); // Y step
  digitalWrite(3, 0);

  pinMode(6, OUTPUT); // Y direction
  digitalWrite(6, 0);

  pinMode(4, OUTPUT); // Z step
  digitalWrite(4, 0);

  pinMode(7, OUTPUT); // Z direction
  digitalWrite(8, 0);

  pinMode(12, OUTPUT); // A step
  digitalWrite(12, 0);

  pinMode(13, OUTPUT); // A direction
  digitalWrite(13, 0);

  pinMode(8, OUTPUT); // Enable
  digitalWrite(9, 0); // LOW to enable
  enable = 1;
}

void resetSteppers() {
  initPins();
  for (char i=0; i<4; i++) {
    trigger[i] = 0;
    direction[i] = 0;
    mode[i] = 0;
    position[i] = 0;
    targetPosition[i] = 0;
  }
}

void initI2C() {
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(i2cRxHandler);
  Wire.onRequest(i2cReqHandler);
}

void run_to_target_time(int i) {
  if (targetTime[i] == 0) {
    trigger[i] = 0;
  } else {
    targetTime[i]--;
  }
}

void run_ramp_time(int i) {
  if (rampUpCounter[i] > 0) {
    speed[i] += rampUpDelta[i];
    if (speed[i] < MIN_SPEED) {
      speed[i] = MIN_SPEED;
    }
    trigger[i] = (1000000 / speed[i]) / 128 - 1;
    counter[i] = 0;
    rampUpCounter[i]--;
  } else if (cruiseCounter[i] > 0) {
    if (speed[i] != cruiseSpeed[i]) {
      speed[i] = cruiseSpeed[i];
      trigger[i] = (1000000 / speed[i]) / 128 - 1;
      counter[i] = 0;
    }
    cruiseCounter[i]--;
  } else if (rampDownCounter[i] > 0) {
    if (speed[i] > rampDownDelta[i]) {
      speed[i] -= rampDownDelta[i];
    } else {
      speed[i] = 0;
    }
    if (speed[i] < MIN_SPEED) {
      speed[i] = MIN_SPEED;
    }
    trigger[i] = (1000000 / speed[i]) / 128 - 1;
    counter[i] = 0;
    rampDownCounter[i]--;
  }
}

void run_ramp_pos(int i) {
  if (rampUpCounter[i] > 0) {
    speed[i] += rampUpDelta[i];
    if (speed[i] < MIN_SPEED) {
      speed[i] = MIN_SPEED;
    }
    trigger[i] = (1000000 / speed[i]) / 128 - 1;
    counter[i] = 0;
    rampUpCounter[i]--;
  } else if ((direction[i] == 0 && position[i] < cruiseEndPosition[i]) || (direction[i] == 1 && position[i] > cruiseEndPosition[i])) {
    if (speed[i] != cruiseSpeed[i]) {
      speed[i] = cruiseSpeed[i];
      trigger[i] = (1000000 / speed[i]) / 128 - 1;
      counter[i] = 0;
    }
  } else if (rampDownCounter[i] > 0) {
    if (speed[i] > rampDownDelta[i]) {
      speed[i] -= rampDownDelta[i];
    } else {
      speed[i] = 0;
    }
    if (speed[i] < MIN_SPEED) {
      speed[i] = MIN_SPEED;
    }
    trigger[i] = (1000000 / speed[i]) / 128 - 1;
    counter[i] = 0;
    rampDownCounter[i]--;
  }
}

void setup() {
  initI2C();
  resetSteppers();
  setupTimer();

  Serial.begin(9600);
}

void loop() {
  if (millis() - last_loop_time > LOOP_PERIOD_MS){
    last_loop_time += LOOP_PERIOD_MS;

    for (char i=0; i<4; i++) {
      if (mode[i] == MODE_RUN_TO_TARGET_TIME) {
        run_to_target_time(i);
      } else if (mode[i] == MODE_RUN_TO_TARGET_TIME_W_RAMP) {
        if (rampDownCounter[i] == 0) {
          trigger[i] = 0;
          mode[i] = MODE_STOP;
        }
        run_ramp_time(i);
      } else if (mode[i] == MODE_RUN_TO_TARGET_POS_W_RAMP) {
        run_ramp_pos(i);
      }
    }
  }
}
