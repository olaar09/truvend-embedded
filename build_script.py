#!/usr/bin/env python3
import requests
import json
import subprocess
import os
import shutil
import sys
import re

if len(sys.argv) != 2:
    print("Usage: python build_script.py <number_of_builds>")
    sys.exit(1)

num_builds = int(sys.argv[1])
url = "http://iot.truvend.online/iot/setup?type=prepaid_meter"

for i in range(num_builds):
    print(f"Building device {i+1}/{num_builds}")
    
    # Call the API
    response = requests.get(url)
    if response.status_code != 200:
        print(f"Failed to get setup data: {response.status_code}")
        continue
    
    data = response.json()
    device_id = data['device_id']
    ssid_new = data['internet_ssid']
    password_new = data['internet_password']
    jwt_new = data['jwt']
    meter_no_new = device_id  # meterNo is device_id
    
    print(f"Device ID: {device_id}")
    
    # Read main.cpp
    with open('src/main.cpp', 'r') as f:
        content = f.read()
    
    # Replace ssid
    content = re.sub(r'const char\* ssid = ".*?";', f'const char* ssid = "{ssid_new}";', content)
    
    # Replace password
    content = re.sub(r'const char\* password = ".*?";', f'const char* password = "{password_new}";', content)
    
    # Replace meterNo
    content = re.sub(r'String meterNo = ".*?";', f'String meterNo = "{meter_no_new}";', content)
    
    # Replace jwtToken
    content = re.sub(r'String jwtToken = ".*?";', f'String jwtToken = "{jwt_new}";', content)
    
    # Write back
    with open('src/main.cpp', 'w') as f:
        f.write(content)
    
    # Run pio run
    print("Running pio run...")
    result = subprocess.run(['pio', 'run'], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Build failed: {result.stderr}")
        continue
    
    # Copy bin files
    build_dir = '.pio/build/esp32doit-devkit-v1'
    output_dir = f'builds/{device_id}'
    os.makedirs(output_dir, exist_ok=True)
    
    shutil.copy(f'{build_dir}/bootloader.bin', output_dir)
    shutil.copy(f'{build_dir}/firmware.bin', output_dir)
    shutil.copy(f'{build_dir}/partitions.bin', output_dir)
    
    print(f"Build completed for {device_id}")

print("All builds completed.")