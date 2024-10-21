#!/usr/bin/env python3
from flask import Flask, render_template, request, jsonify, send_from_directory
import serial
import threading
import time
import datetime
import csv
import os

app = Flask(__name__)

# Global variables
ser = None
data_lock = threading.Lock()
current_reading = None
recording = False
csv_file = None
csv_writer = None

def read_sensor():
    global ser, current_reading, recording, csv_writer, csv_file
    while True:
        try:
            ser.write(b'Z\r\n')
            response = ser.read(100)
            if response:
                reading = response.decode().strip()
                # Remove the percent sign if it exists
                reading = reading.replace('Z', '').strip()
                with data_lock:
                    current_reading = reading
                if recording:
                    timestamp = datetime.datetime.now().isoformat()
                    csv_writer.writerow([timestamp, reading])
                    csv_file.flush()
            else:
                with data_lock:
                    current_reading = None
        except Exception as e:
            print(f"Error reading from sensor: {e}")
            with data_lock:
                current_reading = None
        time.sleep(1)

@app.route('/')
def index():
    # Show the dashboard
    return render_template('index.html')

@app.route('/start_recording', methods=['POST'])
def start_recording():
    global recording, csv_file, csv_writer
    if not recording:
        if not os.path.exists('data'):
            os.makedirs('data')
        filename = datetime.datetime.now().strftime('data/%Y%m%d_%H%M%S.csv')
        csv_file = open(filename, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(['Timestamp', 'Reading'])
        recording = True
    return jsonify({'status': 'recording started'})

@app.route('/stop_recording', methods=['POST'])
def stop_recording():
    global recording, csv_file
    if recording:
        recording = False
        csv_file.close()
        csv_file = None
    return jsonify({'status': 'recording stopped'})

@app.route('/get_reading')
def get_reading():
    with data_lock:
        reading = current_reading
    return jsonify({'reading': reading, 'timestamp': datetime.datetime.now().isoformat()})

@app.route('/list_csv_files')
def list_csv_files():
    if not os.path.exists('data'):
        os.makedirs('data')
    files = os.listdir('data')
    csv_files = [f for f in files if f.endswith('.csv')]
    return jsonify({'files': csv_files})

@app.route('/view_csv_file/<filename>')
def view_csv_file(filename):
    return send_from_directory('data', filename, as_attachment=True)

if __name__ == '__main__':
    # Configure the serial connection (change '/dev/ttyUSB0' and baudrate as needed)
    try:
        ser = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)
        print("Serial connection established")
    except serial.SerialException as e:
        print(f"Error: {e}")
        exit()

    # Start the sensor reading thread
    sensor_thread = threading.Thread(target=read_sensor)
    sensor_thread.daemon = True
    sensor_thread.start()

    app.run(host='0.0.0.0', port=5000)
