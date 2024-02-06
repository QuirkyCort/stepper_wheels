
def read_trig():
    print(struct.unpack('<H', i2c.readfrom_mem(85, 0x41, 2))[0])

def read_dir():
    print(struct.unpack('B', i2c.readfrom_mem(85, 0x45, 1))[0])

def read_mode():
    print(struct.unpack('B', i2c.readfrom_mem(85, 0x49, 1))[0])

def read_pos():
    return struct.unpack('<i', i2c.readfrom_mem(85, 0x4d, 4))[0]

def read_tar():
    print(struct.unpack('<i', i2c.readfrom_mem(85, 0x51, 4))[0])

def set_trig(v):
    i2c.writeto_mem(85, 0x41, struct.pack('<H', v))

def set_dir(v):
    i2c.writeto_mem(85, 0x45, struct.pack('B', v))

def set_mode(v):
    i2c.writeto_mem(85, 0x49, struct.pack('B', v))

def set_pos(v):
    i2c.writeto_mem(85, 0x4d, struct.pack('<i', v))

def set_tar(v):
    i2c.writeto_mem(85, 0x51, struct.pack('<i', v))

def set_speed(pps):
    period_us = 1000000 / pps
    period_trig = round(period_us / 128) - 1
    i2c.writeto_mem(85, 0x41, struct.pack('<H', period_trig))

def stop():
    i2c.writeto_mem(85, 0x41, struct.pack('<H', 0))

def reset():
    i2c.writeto_mem(85, 0x01, struct.pack('B', 1))

def enable():
    i2c.writeto_mem(85, 0x02, struct.pack('B', 1))

def disable():
    i2c.writeto_mem(85, 0x02, struct.pack('B', 0))

# Main

reset()

print('Start continuous move for 2 seconds')
set_speed(200) # Move continuously
time.sleep(2)
stop()
print('stopped')

time.sleep(2)

print('move to -500')
set_tar(-500)
set_dir(1)     # Reverse
set_mode(1)    # Run to target position
set_speed(200) # Start running. It should stop when it reaches the target.

while True:
    p = read_pos()
    print(p)
    if p == -500:
        break
    time.sleep(0.1)
print('stopped')