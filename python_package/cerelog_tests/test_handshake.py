#!/usr/bin/env python3
"""
Test script to manually send handshake packet and verify baud rate switching
"""

import platform
import serial
import time
import struct
import sys

def send_handshake_packet(ser, reg_addr=0x01, reg_val=0x05):
    """Send the exact handshake packet that BrainFlow sends"""
    
    # Get current Unix timestamp
    current_time = int(time.time())
    if current_time < 1600000000:
        current_time = 1500000000  # fallback timestamp
    
    # Build timestamp packet exactly like BrainFlow does
    # [start_marker][msg_type][timestamp][timestamp][timestamp][timestamp][RegAddr][RegVal][checksum][end_marker]
    packet = bytearray(12)
    packet[0] = 0xAA                        # start marker byte 1
    packet[1] = 0xBB                        # start marker byte 2
    packet[2] = 0x02                        # message type
    packet[3] = (current_time >> 24) & 0xFF # timestamp MSB
    packet[4] = (current_time >> 16) & 0xFF # timestamp second byte
    packet[5] = (current_time >> 8) & 0xFF  # timestamp third byte
    packet[6] = current_time & 0xFF         # timestamp LSB
    packet[7] = reg_addr                    # configuration register address
    packet[8] = reg_val                     # configuration register value
    
    # Calculate checksum (same as BrainFlow C++ code)
    checksum = packet[2] + packet[3] + packet[4] + packet[5] + packet[6] + packet[7] + packet[8]
    packet[9] = checksum & 0xFF  # Ensure it's a valid byte
    
    packet[10] = 0xCC                       # end marker byte 1
    packet[11] = 0xDD                       # end marker byte 2
    
    print(f"[HANDSHAKE] Sending packet: {' '.join([f'{b:02X}' for b in packet])}")
    print(f"[HANDSHAKE] Timestamp: {current_time} ({time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(current_time))})")
    print(f"[HANDSHAKE] Reg_addr: 0x{reg_addr:02X}, Reg_val: 0x{reg_val:02X}")
    print(f"[HANDSHAKE] Checksum: 0x{checksum & 0xFF:02X}")
    
    # Send packet
    result = ser.write(packet)
    if result != 12:
        print(f"[ERROR] Failed to send complete packet. Sent {result}/12 bytes")
        return False
    
    ser.flush()  # Ensure packet is sent
    print(f"[SUCCESS] Sent {result} bytes")
    
    return True

def test_handshake():
    """Test the handshake process manually"""
    
    # Determine port and baud rates
    if platform.system() == 'Darwin':  # macOS
        port_name = '/dev/cu.usbserial-110'
        initial_baud = 9600
        target_baud = 230400
    elif platform.system() == 'Windows':
        port_name = 'COM4'
        initial_baud = 9600
        target_baud = 921600
    else:
        port_name = '/dev/ttyUSB0'
        initial_baud = 9600
        target_baud = 921600
    
    print(f"[TEST] Manual Handshake Test")
    print(f"[INFO] Platform: {platform.system()}")
    print(f"[INFO] Port: {port_name}")
    print(f"[INFO] Initial baud: {initial_baud}")
    print(f"[INFO] Target baud: {target_baud}")
    print("-" * 50)
    
    try:
        # Step 1: Connect at initial baud rate
        print(f"[STEP1] Connecting at {initial_baud} baud...")
        ser = serial.Serial(port_name, initial_baud, timeout=1.0)
        print(f"[SUCCESS] Connected to {ser.name}")
        
        # Clear any existing data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(0.1)
        
        # Step 2: Send handshake packet
        print(f"\n[STEP2] Sending handshake packet...")
        if not send_handshake_packet(ser, reg_addr=0x01, reg_val=0x05):  # 0x05 = 230400 baud
            ser.close()
            return False
        
        # Step 3: Wait for response
        print(f"\n[STEP3] Waiting for response...")
        time.sleep(0.2)  # Wait for response
        
        # Check for response
        if ser.in_waiting > 0:
            response = ser.read(ser.in_waiting)
            print(f"[RESPONSE] Got {len(response)} bytes: {response.hex()}")
            
            # Check for "OK" response
            if b'OK' in response:
                print(f"[SUCCESS] Received 'OK' response!")
            else:
                print(f"[WARNING] No 'OK' response found")
        else:
            print(f"[WARNING] No response received")
        
        # Step 4: Switch to target baud rate
        print(f"\n[STEP4] Switching to {target_baud} baud...")
        ser.close()
        time.sleep(0.1)
        
        ser = serial.Serial(port_name, target_baud, timeout=1.0)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(0.1)
        
        # Step 5: Check for data at new baud rate
        print(f"\n[STEP5] Checking for data at {target_baud} baud...")
        data_count = 0
        start_time = time.time()
        
        while time.time() - start_time < 3:  # Check for 3 seconds
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                data_count += len(data)
                print(f"[DATA] Got {len(data)} bytes: {data[:16].hex()}")
            time.sleep(0.1)
        
        if data_count > 0:
            print(f"[SUCCESS] Received {data_count} bytes at {target_baud} baud!")
            print(f"[SUCCESS] Handshake and baud rate switching worked!")
        else:
            print(f"[ERROR] No data received at {target_baud} baud")
            print(f"[INFO] Board might not have switched baud rates properly")
        
        ser.close()
        return data_count > 0
        
    except serial.SerialException as e:
        print(f"[ERROR] Serial error: {e}")
        return False
    except Exception as e:
        print(f"[ERROR] Unexpected error: {e}")
        return False

if __name__ == "__main__":
    success = test_handshake()
    
    if success:
        print(f"\n[SUCCESS] Handshake test passed!")
        print(f"[INFO] The board is properly switching baud rates")
    else:
        print(f"\n[FAILED] Handshake test failed!")
        print(f"[INFO] Check the board firmware and connections")
    
    sys.exit(0 if success else 1) 