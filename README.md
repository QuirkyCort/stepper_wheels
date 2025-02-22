# stepper_controller
Firmware for controlling steppers on a "CNC Shield V3.0".

This firmware is intended to make the attached steppers behave like robot wheels.
A main controller should be able to issue commands to the stepper controller, and have it move to a target position at a specified speed asynchronously.

## I2C Address
The I2C client address for the stepper controller is 0x55.
This can be changed by modifying ```I2C_ADDRESS``` in the code.

## Register Map
Unlike most I2C memory device where each register address maps to a single byte, this controller maps each address to a variable number of bytes.
Examples...

* 0x00 is a UInt8 * 3 (3 bytes) address. You'll need to read / write 3 bytes at a time when using this address.
Each byte is a UInt8 value.

* 0x41 is a Int16 (2 bytes) address. You'll need to read / write 2 bytes at a time when using this address.
If you read 0x42, you will NOT get the higher or lower byte of 0x41.

### Version (0x00) (UInt8 * 3) (Read-Only)
* UInt8: Major Version
* UInt8: Minor Version
* UInt8: Patch Version

### Reset (0x01) (UInt8) (Write-Only)
Reset if a 1 is written.

### Enable (0x02) (UInt8) (Write-Only)
1 to enable (default), 0 to disable.
If disabled, the steppers will be free to rotate.
If enabled, the steppers will be held in place.

### Stop (0x20, 0x21, 0x22, 0x23) (UInt8) (Write-Only)
Each address correspond to one stepper (X, Y, Z, A).

Write 1 to stop the stepper immediately.
Any other values will have no effects.

### Run continuous (0x30, 0x31, 0x32, 0x33) (float) (Write-Only)
Each address correspond to one stepper (X, Y, Z, A).

Run the stepper immediately at the specified speed (steps/sec) without acceleration.
The stepper will continue running until given a different command.

### Run to target without ramp (0x40, 0x41, 0x42, 0x43) (float, UInt8, Int32) (Write-Only)
Each address correspond to one stepper (X, Y, Z, A).

* float: Speed (steps/sec)
* UInt8: Target Position Type
* Int32: Target Position

Run the stepper at the specified speed (without acceleration) to the target position.
If target position type is set to 1, the target position will be relative to the current position.

### Run to target with ramp (0x50, 0x51, 0x52, 0x53) (float, UInt8, Int32) (Write-Only)
Each address correspond to one stepper (X, Y, Z, A).

* float: Speed (steps/sec)
* UInt8: Target Position Type
* Int32: Target Position

Run the stepper with acceleration to the target position.
The stepper motor will ramp up the speed at the start, and ramp down at the end.
If target position type is set to 1, the target position will be relative to the current position.

### Position (0x60, 0x61, 0x62, 0x63) (Int32) (Read/Write)
Each address correspond to one stepper (X, Y, Z, A).

Position of the stepper in steps.
This value can be positive or negative (...for movement in the reverse direction).
Can be written to set the position to zero or any other value.

### Speed (0x70, 0x71, 0x72, 0x73) (float) (Read-Only)
Each address correspond to one stepper (X, Y, Z, A).

Current speed of the stepper in steps per second.
This value can be positive or negative (...for movement in the reverse direction).

### Acceleration (0x80, 0x81, 0x82, 0x83) (float) (Write-Only)
Each address correspond to one stepper (X, Y, Z, A).

Sets the acceleration of the stepper in steps per second squared.
The polarity of this value doesn't matter (ie. setting to -2 is the same as setting to 2).

### Running (0x90, 0x91, 0x92, 0x93) (UInt8) (Read-Only)
Each address correspond to one stepper (X, Y, Z, A).

Returns 1 if the stepper is running and 0 if it is not.
Note: When using Run Continuous, this will return 1 even if the speed is set to zero.
