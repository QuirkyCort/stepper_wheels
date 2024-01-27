/*
 * Register Map
 * ===
 * Unlike most I2C memory device where each register address maps to a single byte,
 * this controller maps each address to a variable number of bytes.
 * Eg. 0x41 is a Int16 (2 bytes) address. You'll need to read / write 2 bytes at
 * a time when using this address. If you read 0x42, you will NOT get the higher or
 * lower byte of 0x41.
 *
 * Read Only
 * ===
 * 0x00 (2 x Int8): Version. Major, Minor.
 * 0x01 (Int8): Reset if a 1 is written.
 * 0x02 (Int8): 1 to enable (default), 0 to disable
 *
 * Read Write
 * ===
 * 0x41 (Int16): Stepper 1 Trigger.
 * 0x42 (Int16): Stepper 2 Trigger.
 * 0x43 (Int16): Stepper 3 Trigger.
 * 0x44 (Int16): Stepper 4 Trigger.
 * 0 means never trigger. 1 is the minimum trigger.
 * The trigger controls the period between each step.
 * Period = (trigger + 1) * 128us
 *
 * 0x45 (Int8): Stepper 1 Direction.
 * 0x46 (Int8): Stepper 2 Direction.
 * 0x47 (Int8): Stepper 3 Direction.
 * 0x48 (Int8): Stepper 4 Direction.
 * 0 means forward, 1 means reverse.
 *
 * 0x49 (Int8): Stepper 1 Mode.
 * 0x4A (Int8): Stepper 2 Mode.
 * 0x4B (Int8): Stepper 3 Mode.
 * 0x4C (Int8): Stepper 4 Mode.
 * 0 means "run continuous", 1 means "run till target position".
 *
 * 0x4D (Int32): Stepper 1 Position.
 * 0x4E (Int32): Stepper 2 Position.
 * 0x4F (Int32): Stepper 3 Position.
 * 0x50 (Int32): Stepper 4 Position.
 *
 * 0x51 (Int32): Stepper 1 Target Position.
 * 0x52 (Int32): Stepper 2 Target Position.
 * 0x53 (Int32): Stepper 3 Target Position.
 * 0x54 (Int32): Stepper 4 Target Position.
 */

#include <Wire.h>

byte VERSION[] = {1, 1, 1};

volatile uint8_t registerPtr = 0;

volatile uint16_t stepperCounter[4];

volatile uint16_t stepperTrigger[4];
volatile uint8_t stepperDirection[4];
volatile uint8_t stepperMode[4];
volatile int32_t stepperPosition[4];
volatile int32_t stepperTargetPosition[4];


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
    } else {
      PORTB |= B00000001;
    }

  // Trigger
  } else if (registerPtr >= 0x41 && registerPtr <= 0x44 && numBytes == 3) {
    byte *bytes = (byte*)&stepperTrigger[registerPtr - 0x41];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    stepperCounter[registerPtr - 0x41] = 0;

  // Direction
  } else if (registerPtr >= 0x45 && registerPtr <= 0x48 && numBytes == 2) {
    stepperDirection[registerPtr - 0x45] = Wire.read();
    if (registerPtr == 0x45) {
      if (stepperDirection[0] == 1) {
        PORTD |= B00100000;
      } else {
        PORTD &= B11011111;
      }
    } else if (registerPtr == 0x46) {
      if (stepperDirection[1] == 1) {
        PORTD |= B01000000;
      } else {
        PORTD &= B10111111;
      }
    } else if (registerPtr == 0x47) {
      if (stepperDirection[2] == 1) {
        PORTD |= B10000000;
      } else {
        PORTD &= B01111111;
      }
    } else if (registerPtr == 0x48) {
      if (stepperDirection[3] == 1) {
        PORTB |= B00100000;
      } else {
        PORTB &= B11011111;
      }
    }

  // Mode
  } else if (registerPtr >= 0x49 && registerPtr <= 0x4C && numBytes == 2) {
    stepperMode[registerPtr - 0x49] = Wire.read();

  // Position
  } else if (registerPtr >= 0x4D && registerPtr <= 0x50 && numBytes == 5) {
    byte *bytes = (byte*)&stepperPosition[registerPtr - 0x4D];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

  // Target Position
  } else if (registerPtr >= 0x51 && registerPtr <= 0x54 && numBytes == 5) {
    byte *bytes = (byte*)&stepperTargetPosition[registerPtr - 0x51];
    bytes[0] = Wire.read();
    bytes[1] = Wire.read();
    bytes[2] = Wire.read();
    bytes[3] = Wire.read();

  }

}

void i2cReqHandler(void) {
  if (registerPtr == 0) {
    Wire.write(VERSION, 3);

  // Trigger
  } else if (registerPtr >= 0x41 && registerPtr <= 0x44) {
    Wire.write((byte*)&stepperTrigger[registerPtr - 0x41], 2);

  // Direction
  } else if (registerPtr >= 0x45 && registerPtr <= 0x48) {
    Wire.write(stepperDirection[registerPtr - 0x45]);

  // Mode
  } else if (registerPtr >= 0x49 && registerPtr <= 0x4C) {
    Wire.write(stepperDirection[registerPtr - 0x49]);

  // Position
  } else if (registerPtr >= 0x4D && registerPtr <= 0x50) {
    Wire.write((byte*)&stepperPosition[registerPtr - 0x4D], 4);

  // Target Position
  } else if (registerPtr >= 0x51 && registerPtr <= 0x54) {
    Wire.write((byte*)&stepperTargetPosition[registerPtr - 0x51], 4);

  }
}

// Loop unrolled to save clock cycles. Not sure if that's really needed.
ISR(TIMER2_OVF_vect) {
  if (stepperTrigger[0] && stepperCounter[0] == stepperTrigger[0]) {
    PORTD |= B00000100;
    stepperCounter[0] = 0;
    if (stepperDirection[0] == 0) {
      stepperPosition[0]++;
    } else {
      stepperPosition[0]--;
    }
    if (stepperMode[0] == 1) {
      if (stepperDirection[0] == 0) {
        if (stepperPosition[0] >= stepperTargetPosition[0]) {
          stepperTrigger[0] = 0;
        }
      } else {
        if (stepperPosition[0] <= stepperTargetPosition[0]) {
          stepperTrigger[0] = 0;
        }
      }
    }
  } else {
    PORTD &= B11111011;
  }

  if (stepperTrigger[1] && stepperCounter[1] == stepperTrigger[1]) {
    PORTD |= B00001000;
    stepperCounter[1] = 0;
    if (stepperDirection[1] == 0) {
      stepperPosition[1]++;
    } else {
      stepperPosition[1]--;
    }
    if (stepperMode[1] == 1) {
      if (stepperDirection[1] == 0) {
        if (stepperPosition[1] >= stepperTargetPosition[1]) {
          stepperTrigger[1] = 0;
        }
      } else {
        if (stepperPosition[1] <= stepperTargetPosition[1]) {
          stepperTrigger[1] = 0;
        }
      }
    }
  } else {
    PORTD &= B11110111;
  }

  if (stepperTrigger[2] && stepperCounter[2] == stepperTrigger[2]) {
    PORTD |= B00010000;
    stepperCounter[2] = 0;
    if (stepperDirection[2] == 0) {
      stepperPosition[2]++;
    } else {
      stepperPosition[2]--;
    }
    if (stepperMode[2] == 1) {
      if (stepperDirection[2] == 0) {
        if (stepperPosition[2] >= stepperTargetPosition[2]) {
          stepperTrigger[2] = 0;
        }
      } else {
        if (stepperPosition[2] <= stepperTargetPosition[2]) {
          stepperTrigger[2] = 0;
        }
      }
    }
  } else {
    PORTD &= B11101111;
  }

  if (stepperTrigger[3] && stepperCounter[3] == stepperTrigger[3]) {
    PORTB |= B00010000;
    stepperCounter[3] = 0;
    if (stepperDirection[3] == 0) {
      stepperPosition[3]++;
    } else {
      stepperPosition[3]--;
    }
    if (stepperMode[3] == 1) {
      if (stepperDirection[3] == 0) {
        if (stepperPosition[3] >= stepperTargetPosition[3]) {
          stepperTrigger[3] = 0;
        }
      } else {
        if (stepperPosition[3] <= stepperTargetPosition[3]) {
          stepperTrigger[3] = 0;
        }
      }
    }
  } else {
    PORTD &= B11101111;
  }

  stepperCounter[0]++;
  stepperCounter[1]++;
  stepperCounter[2]++;
  stepperCounter[3]++;
}

void setupTimer() {
  TCCR2A = 0;           // Init Timer1A
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

}

void resetSteppers() {
  initPins();
  for (char i=0; i<4; i++) {
    stepperTrigger[i] = 0;
    stepperDirection[i] = 0;
    stepperMode[i] = 0;
    stepperPosition[i] = 0;
    stepperTargetPosition[i] = 0;
  }
}

void initI2C() {
  Wire.begin(0x55); // Initialize I2C (Slave Mode: address=0x55 )
  Wire.onReceive(i2cRxHandler);
  Wire.onRequest(i2cReqHandler);
}

void setup() {
  initI2C();
  resetSteppers();
  setupTimer();
}

void loop() {
}
