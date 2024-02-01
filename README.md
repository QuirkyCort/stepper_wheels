# stepper_controller
Firmware for controlling steppers on a "CNC Shield V3.0".

This firmware is intended to make the attached steppers behave like robot wheels.
A main controller should be able to issue commands to the stepper controller, and have it move to a target position at a specified speed asynchronously.

## I2C Address
The I2C client address for the stepper controller is 0x55.
This can be changed by modifying ```I2C_ADDRESS``` in the code.

## Register Map
Unlike most I2C memory device where each register address maps to a single byte, this controller maps each address to a variable number of bytes.
Eg. 0x0 is a UInt8 * 3 (3 bytes) address. You'll need to read / write 3 bytes at a time when using this address. Each byte is a UInt8 value.
Eg. 0x41 is a Int16 (2 bytes) address. You'll need to read / write 2 bytes at a time when using this address.
If you read 0x42, you will NOT get the higher or lower byte of 0x41.

### Version (0x00) (UInt8 * 3) (Read-Only)
* UInt8: Major Version
* UInt8: Minor Version
* UInt8: Patch Version

### Reset (0x01) (UInt8) (Write-Only)
Reset if a 1 is written.

### Enable (0x02) (UInt8)
1 to enable (default), 0 to disable
If disabled, the steppers will be free to rotate.
If enabled, the steppers will be held in place.

### Trigger (0x41, 0x42, 0x43, 0x44) (UInt16)
Each address correspond to one stepper (X, Y, Z, A).

The trigger controls the period between each step (stepper movement).
When set to 0, the stepper will not step (ie. it'll hold in place).

```
Period = (Trigger + 1) * 128us
```

Given the desired speed (...in steps per second), the trigger can be calculated by...

```
Period(us) = 1000000 / speed(steps per sec)
Trigger = Period / 128 - 1  # Round to an UInt16 value
```

### Direction (0x45, 0x46, 0x47, 0x48) (UInt8)
Each address correspond to one stepper (X, Y, Z, A).

Stepper direction.
0 means forward, 1 means reverse.

### Mode (0x49, 0x4A, 0x4B, 0x4C) (Uint8)
Each address correspond to one stepper (X, Y, Z, A).

Run mode:
* 0 : "stop"
* 1 : "run continuous"
* 2 : "run till target time"
* 20 : "run till target position"
* 21 : "run till target position with ramp"

### Position (0x4D, 0x4E, 0x4F, 0x50) (Int32)
Each address correspond to one stepper (X, Y, Z, A).

Position of the stepper in steps.
This value can be positive or negative (...for movement in the reverse direction).
Can be written to set the position to zero or any other value.

### Target Position (0x51, 0x52, 0x53, 0x54) Write: (UInt8, Int32), Read: (Int32)
Each address correspond to one stepper (X, Y, Z, A).

* UInt8: Target Position Type
* Int32: Target Position

If Target Position Type is set to 0 (absolute), the Target Position will set based on the given value.
If the Target Position Type is set to 1 (relative), the current position will be added to the given value.

When in "run continuous" mode, this value is ignored.
When in "run till target position" or "run till target position with ramp" mode, the stepper will be automatically stopped, and trigger set to zero when this position is reached.

When reading this address, only the Target Position (Int32) is read and the value is always absolute.

### Target Time (0x55, 0x56, 0x57, 0x58) (UInt16)
Each address correspond to one stepper (X, Y, Z, A).

Target time to run in units of 100ms (ie. if set to 20, the stepper will run for 2000ms).
Only used in "run till target time" mode.

When read, it provides the remaining time to run.

### Target Position with Ramp (0x59, 0x5A, 0x5B, 0x5C) (Int32, UInt16 * 6)
Each address correspond to one stepper (X, Y, Z, A).

The values to write are...

* Int32: Target Position
* UInt16: Ramp-up Counter (units of 100ms)
* UInt16: Ramp-up Delta (amount added to speed at each ramp-up interval)
* UInt16: Cruise Counter (units of 100ms)
* UInt16: Cruise Speed (steps per second)
* UInt16: Ramp-down Counter (units of 100ms)
* UInt16: Ramp-down Delta (amount subtracted from speed at each ramp-down interval)

Target position is always relative to the current position.
When the target position is reached, the stepper will stop.

Note that speed is always positive; if moving in reverse, direction must be set separately.

Every 100ms, the stepper will increase its speed by "Ramp-up Delta" and reduce "Ramp-up Counter" by one, until "Ramp-up Counter" reaches zero.

The stepper will then run at "Cruise Speed" for "Cruise Counter" * 100ms.

Finally, the will reduce its speed by "Ramp-down Delta" and reduce "Ramp-down Counter" by one every 100ms, until "Ramp-down Counter" reaches zero.
Speed will never drop below MIN_SPEED.

This address is write-only, but the target position may be read using the Target Position address.
The speed may also be determined at any time by reading Trigger and calculating speed from it.
