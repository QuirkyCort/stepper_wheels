from micropython import const
import struct
import time

DEFAULT_ACCELERATION = const(50)
TIME_STEP_MS = const(100)
WAIT_MS = const(50)

VERSION_REG = const(0x00)
RESET_REG = const(0x01)
ENABLE_REG = const(0x02)
TRIGGER_REG = const(0x41)
DIRECTION_REG = const(0x45)
MODE_REG = const(0x49)
POSITION_REG = const(0x4D)
TARGET_POSITION_REG = const(0x51)
TARGET_TIME_REG = const(0x55)
TARGET_POSITION_WITH_RAMP_REG = const(0x59)
TARGET_TIME_WITH_RAMP_REG = const(0x5D)

MODE_STOP = const(0)
MODE_RUN_CONTIUOUS = const(1)
MODE_RUN_TILL_TIME = const(20)
MODE_RUN_TILL_TIME_WITH_RAMP = const(21)
MODE_RUN_TILL_POSITION = const(30)
MODE_RUN_TILL_POSITION_WITH_RAMP = const(31)


class Motor:
    def __init__(self, controller, wheel):
        self.controller = controller
        self.wheel = wheel
        self.acceleration_up = DEFAULT_ACCELERATION
        self.acceleration_down = DEFAULT_ACCELERATION

    def set_trigger(self, trigger):
        addr = self.wheel + TRIGGER_REG
        self.controller.write(addr, struct.pack('<H', trigger))

    def get_trigger(self):
        addr = self.wheel + TRIGGER_REG
        return struct.unpack('<H', self.controller.read(addr, 2))[0]

    def set_direction(self, direction):
        addr = self.wheel + DIRECTION_REG
        self.controller.write(addr, struct.pack('B', direction))

    def get_direction(self):
        addr = self.wheel + DIRECTION_REG
        return struct.unpack('B', self.controller.read(addr, 1))[0]

    def set_mode(self, mode):
        addr = self.wheel + MODE_REG
        self.controller.write(addr, struct.pack('B', mode))

    def get_mode(self):
        addr = self.wheel + MODE_REG
        return struct.unpack('B', self.controller.read(addr, 1))[0]

    def set_position(self, position):
        addr = self.wheel + POSITION_REG
        self.controller.write(addr, struct.pack('<i', position))

    def get_position(self):
        addr = self.wheel + POSITION_REG
        return struct.unpack('<i', self.controller.read(addr, 4))[0]

    def set_target_position(self, position, relative=True):
        addr = self.wheel + TARGET_POSITION_REG
        if relative:
            pos_type = 1
        else:
            pos_type = 0
        self.controller.write(addr, struct.pack('<Bi', pos_type, position))

    def get_target_position(self):
        addr = self.wheel + TARGET_POSITION_REG
        return struct.unpack('<i', self.controller.read(addr, 4))[0]

    def set_target_time(self, time_steps):
        addr = self.wheel + TARGET_TIME_REG
        self.controller.write(addr, struct.pack('<H', time_steps))

    def get_target_time(self):
        addr = self.wheel + TARGET_TIME_REG
        return struct.unpack('<H', self.controller.read(addr, 2))[0]

    def set_target_position_with_ramp(self, position, up_count, up_delta, cruise_count, cruise_speed, down_count, down_delta):
        addr = self.wheel + TARGET_POSITION_WITH_RAMP_REG
        data = struct.pack(
            '<iHHHHHH',
            int(position),
            int(up_count),
            int(up_delta),
            int(cruise_count),
            int(cruise_speed),
            int(down_count),
            int(down_delta)
        )
        self.controller.write(addr, data)

    def set_target_time_with_ramp(self, up_count, up_delta, cruise_count, cruise_speed, down_count, down_delta):
        addr = self.wheel + TARGET_TIME_WITH_RAMP_REG
        data = struct.pack(
            '<HHHHHH',
            int(up_count),
            int(up_delta),
            int(cruise_count),
            int(cruise_speed),
            int(down_count),
            int(down_delta)
        )
        self.controller.write(addr, data)

    # Following methods are derived from above methods

    def wait_till_stop(self):
        while self.get_trigger() != 0:
            time.sleep_ms(WAIT_MS)

    def set_speed(self, speed):
        if speed >=0:
            direction = 0
        else:
            direction = 1
            speed = -speed

        period = 1000000 / speed
        trigger = round(period / 128 - 1)
        self.set_direction(direction)
        self.set_trigger(trigger)

    def get_speed(self):
        trigger = self.get_trigger()
        if trigger == 0:
            return 0
        direction = self.get_direction()
        period = (trigger + 1) * 128
        speed = round(1000000 / period)

        if direction >= 0:
            return speed
        else:
            return -speed

    def _run_time_no_ramp(self, speed, time):
        self.set_speed(speed)
        self.set_target_time(round(time * 1000 / TIME_STEP_MS))
        self.set_mode(MODE_RUN_TILL_TIME)

    def _run_time_ramp(self, speed, time):
        if speed < 0:
            direction = 1
        else:
            direction = 0
        speed = abs(speed)
        self.set_direction(direction)
        self.set_target_time_with_accel(speed, time)
        self.set_mode(MODE_RUN_TILL_TIME_WITH_RAMP)

    def set_target_time_with_accel(self, speed, time):
        total_time_steps = round(time * 1000 / TIME_STEP_MS)
        up_count = speed // self.acceleration_up
        up_delta = speed // up_count
        down_count = speed // self.acceleration_down
        down_delta = speed // down_count
        cruise_count = total_time_steps - up_count - down_count
        cruise_speed = speed
        self.set_target_time_with_ramp(up_count, up_delta, cruise_count, cruise_speed, down_count, down_delta)

    def _run_angle_no_ramp(self, speed, angle):
        angle = abs(angle)
        if speed < 0:
            angle = -angle
        self.set_speed(speed)
        self.set_target_position(angle)
        self.set_mode(MODE_RUN_TILL_POSITION)

    def _run_angle_ramp(self, speed, angle):
        if speed * angle < 0:
            angle = -abs(angle)
            direction = 1
        else:
            angle = abs(angle)
            direction = 0
        speed = abs(speed)

        self.set_direction(direction)
        self.set_target_position_with_accel(speed, angle)
        self.set_mode(MODE_RUN_TILL_POSITION_WITH_RAMP)

    def set_target_position_with_accel(self, speed, position):
        up_count = speed // self.acceleration_up
        up_delta = speed // up_count
        down_count = speed // self.acceleration_down
        down_delta = speed // down_count
        up_ramp_dist = (up_delta + speed) // 2 * up_count * TIME_STEP_MS // 1000
        down_ramp_dist = (down_delta + speed) // 2 * down_count * TIME_STEP_MS // 1000
        cruise_count = (abs(position) - up_ramp_dist - down_ramp_dist) * 1000 // TIME_STEP_MS // speed
        cruise_speed = speed
        print(position, up_count, up_delta, cruise_count, cruise_speed, down_count, down_delta)
        self.set_target_position_with_ramp(position, up_count, up_delta, cruise_count, cruise_speed, down_count, down_delta)

    # User facing methods.
    # While the above methods may be public, the user should avoid using them and use the below methods instead.

    def stop(self):
        self.set_trigger(0)
        self.set_mode(MODE_STOP)

    def run(self, speed):
        self.set_speed(speed)
        self.set_mode(MODE_RUN_CONTIUOUS)

    def speed(self):
        return self.get_speed()

    def run_time(self, speed, time, ramp=True, wait=True):
        if ramp:
            self._run_time_ramp(speed, time)
        else:
            self._run_time_no_ramp(speed, time)
        if wait:
            self.wait_till_stop()

    def run_angle(self, speed, angle, ramp=True, wait=True):
        if ramp:
            self._run_angle_ramp(speed, angle)
        else:
            self._run_angle_no_ramp(speed, angle)

        if wait:
            self.wait_till_stop()


class MoveTank:
    def __init__(self, left_motor, right_motor):
        try:
            iter(left_motor)
            self.left_motors = left_motor
        except:
            self.left_motors = (left_motor)

        try:
            iter(right_motor)
            self.right_motors = right_motor
        except:
            self.right_motors = (right_motor)

    def run(self, left_speed, right_speed):
        for motor in self.left_motors:
            motor.run(left_speed)

        for motor in self.right_motors:
            motor.run(right_speed)

    def run_angle(self, left_speed, right_speed, angle):
        left_speed_abs = abs(left_speed)
        right_speed_abs = abs(right_speed)

        if left_speed_abs > right_speed_abs:
            minor_angle = right_speed_abs / left_speed_abs * angle
        else:
            minor_angle = left_speed_abs / right_speed_abs * angle



class Controller:
    def __init__(self, i2c, addr=0x55):
        self.i2c = i2c
        self.addr = addr
        self.reset()

    def read(self, reg, size):
        return self.i2c.readfrom_mem(self.addr, reg, size)

    def write(self, reg, data):
        self.i2c.writeto_mem(self.addr, reg, data)

    def get_version(self):
        return struct.unpack('BBB', self.read(VERSION_REG, 3))

    def reset(self):
        self.write(RESET_REG, b'\x01')

    def enable(self):
        self.write(ENABLE_REG, b'\x01')

    def disable(self):
        self.write(ENABLE_REG, b'\x00')

    def get_motor(self, wheel):
        return Motor(self, wheel)