#!/usr/bin/env python3
import serial

# Configure the serial connection (change '/dev/ttyUSB0' and baudrate as needed)
try:
    ser = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)
    print("Serial connection established")
except serial.SerialException as e:
    print(f"Error: {e}")
    exit()

# Send the string '%\r\n'
try:
    ser.write(b'%\r\n')
    print("Data sent: %\\r\\n")
except Exception as e:
    print(f"Error writing to serial port: {e}")

# Try to read a response (if applicable)
response = ser.read(100)  # Read up to 100 bytes or until the timeout
if response:
    print("Received response:", response.decode())
else:
    print("No response received")

# Close the serial connection
ser.close()
