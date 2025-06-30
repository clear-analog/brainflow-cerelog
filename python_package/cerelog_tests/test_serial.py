#!/usr/bin/env python3
"""
Raw Serial Test for Cerelog X8 Board
Tests direct serial communication without Brainflow layer
"""

import platform
import serial
import time
import sys
import struct
from typing import Optional, Tuple
from brainflow.board_shim import BrainFlowInputParams, BoardShim, BoardIds, LogLevels

# Packet format constants (from ESP32 code)
PACKET_TOTAL_SIZE = 37
START_MARKER = 0xABCD  # 2 bytes
END_MARKER = 0xDCBA    # 2 bytes
PACKET_MSG_LENGTH = 31  # timestamp (4) + ADS1299 data (27)

# Packet indices
PACKET_IDX_START_MARKER = 0
PACKET_IDX_LENGTH = 2
PACKET_IDX_TIMESTAMP = 3
PACKET_IDX_ADS1299_DATA = 7
PACKET_IDX_CHECKSUM = 34
PACKET_IDX_END_MARKER = 35

class PacketParser:
    def __init__(self):
        self.valid_packets = 0
        self.invalid_packets = 0
        self.partial_packets = 0
        self.checksum_errors = 0
        
    def parse_packet(self, data: bytes) -> Optional[dict]:
        """Parse a single packet and return structured data"""
        if len(data) != PACKET_TOTAL_SIZE:
            self.partial_packets += 1
            return None
            
        # Check start marker
        start = struct.unpack('>H', data[PACKET_IDX_START_MARKER:PACKET_IDX_START_MARKER+2])[0]
        if start != START_MARKER:
            self.invalid_packets += 1
            return None
            
        # Check end marker
        end = struct.unpack('>H', data[PACKET_IDX_END_MARKER:PACKET_IDX_END_MARKER+2])[0]
        if end != END_MARKER:
            self.invalid_packets += 1
            return None
            
        # Extract fields
        length = data[PACKET_IDX_LENGTH]
        timestamp = struct.unpack('>L', data[PACKET_IDX_TIMESTAMP:PACKET_IDX_TIMESTAMP+4])[0]
        
        # Verify checksum
        calculated_checksum = sum(data[PACKET_IDX_LENGTH:PACKET_IDX_CHECKSUM]) & 0xFF
        received_checksum = data[PACKET_IDX_CHECKSUM]
        
        if calculated_checksum != received_checksum:
            self.checksum_errors += 1
            return None
            
        # Parse ADS1299 data (27 bytes: 3 status + 8 channels * 3 bytes each)
        ads_data = data[PACKET_IDX_ADS1299_DATA:PACKET_IDX_ADS1299_DATA+27]
        status_bytes = ads_data[0:3]
        
        # Parse 8 channels (3 bytes each, 24-bit signed)
        channels = []
        for i in range(8):
            start_idx = 3 + i * 3  # Skip 3 status bytes
            if start_idx + 3 <= len(ads_data):
                # Convert 3 bytes to signed 24-bit value
                raw_bytes = ads_data[start_idx:start_idx+3]
                # Pad to 4 bytes for struct.unpack
                padded = b'\x00' + raw_bytes if raw_bytes[0] < 0x80 else b'\xFF' + raw_bytes
                value = struct.unpack('>i', padded)[0]
                
                # Convert to voltage (based on your C++ conversion)
                gain = 24
                vref = 4.5
                volts = value * ((2.0 * vref / gain) / (1 << 24))
                channels.append(volts)
        
        self.valid_packets += 1
        return {
            'timestamp': timestamp,
            'length': length,
            'channels': channels,
            'status_bytes': status_bytes.hex(),
            'checksum_ok': True
        }
    
    def print_stats(self):
        total = self.valid_packets + self.invalid_packets + self.partial_packets + self.checksum_errors
        if total > 0:
            print(f"\n=== Packet Statistics ===")
            print(f"Valid packets:    {self.valid_packets:4d} ({self.valid_packets/total*100:.1f}%)")
            print(f"Invalid markers:  {self.invalid_packets:4d} ({self.invalid_packets/total*100:.1f}%)")
            print(f"Checksum errors:  {self.checksum_errors:4d} ({self.checksum_errors/total*100:.1f}%)")
            print(f"Partial packets:  {self.partial_packets:4d} ({self.partial_packets/total*100:.1f}%)")
            print(f"Total processed:  {total:4d}")

def find_packet_boundaries(buffer: bytearray) -> list:
    """Find packet start position in buffer"""
    positions = []
    for i in range(len(buffer) - 1):
        if buffer[i] == 0xAB and buffer[i + 1] == 0xCD:
            positions.append(i)
    return positions

def test_serial_connection():
    """Test raw serial communication with Cerelog board"""
    # Set port and baud rate based on OS for direct serial access
    # TODO add port scanning like in cerelog.cpp
    baud_rate = 9600
    port_name = None
    
    # Auto-detect port on macOS/Linux
    if platform.system() in ['Darwin', 'Linux']:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            if 'usbserial' in port.device:
                port_name = port.device
                break
    
    if not port_name:
        print("[INPUT] Manual port selection required")
        port_name = input("Enter serial port: ")
        
    print(f"[CONNECTION] Testing serial connection to Cerelog X8")
    print(f"   Using port: {port_name} on {platform.system()}")
    print(f"   Testing at: {baud_rate} baud")
    print(f"   Expected packet size: {PACKET_TOTAL_SIZE} bytes")
    
    try:
        # Connect at 9600 baud
        print(f"[INFO] Connecting at {baud_rate} baud...")
        ser = serial.Serial(port_name, baud_rate, timeout=1.0)
        time.sleep(0.1)
        print(f"[SUCCESS] Opened serial port: {ser.name}")
        
        # Clear buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(0.5)
        
        # Check if we're getting data
        initial_data_count = 0
        start_check = time.time()
        while time.time() - start_check < 2:  # Check for 2 seconds
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                initial_data_count += len(data)
            time.sleep(0.1)
        
        if initial_data_count > 0:
            print(f"[INFO] Got {initial_data_count} bytes at {baud_rate} baud")
            print(f"[INFO] Board is communicating at default baud rate")
        else:
            print(f"[WARNING] No data received at {baud_rate} baud")
            print(f"[INFO] Board might not be sending data or needs reset")
        
        print(f"\n[DATA] Starting data collection at {baud_rate} baud...")
        parser = PacketParser()
        buffer = bytearray()
        start_time = time.time()
        last_stats_time = start_time
        
        # Collect data for 5 seconds
        while time.time() - start_time < 5:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                buffer.extend(data)
                
                # Process complete packets
                while len(buffer) >= PACKET_TOTAL_SIZE:
                    boundaries = find_packet_boundaries(buffer)
                    
                    if not boundaries:
                        # No start marker found, discard some data
                        buffer = buffer[1:]
                        continue
                    
                    start_pos = boundaries[0]
                    
                    # If start marker is not at the beginning, discard preceding data
                    if start_pos > 0:
                        buffer = buffer[start_pos:]
                        continue
                    
                    # Check if we have a complete packet
                    if len(buffer) >= PACKET_TOTAL_SIZE:
                        packet_data = bytes(buffer[:PACKET_TOTAL_SIZE])
                        buffer = buffer[PACKET_TOTAL_SIZE:]
                        
                        # Parse the packet
                        packet = parser.parse_packet(packet_data)
                        if packet:
                            # Print occasional packet info
                            if parser.valid_packets % 50 == 1:
                                print(f"\n[PACKET] Packet #{parser.valid_packets}")
                                print(f"   Timestamp: {packet['timestamp']}")
                                print(f"   Channels: {[f'{ch:.6f}V' for ch in packet['channels'][:4]]}...")
                                print(f"   Status: {packet['status_bytes']}")
            
            # Print stats every 1 seconds
            if time.time() - last_stats_time > 1:
                if parser.valid_packets > 0:
                    rate = parser.valid_packets / (time.time() - start_time)
                    print(f"\n[STATS] Rate: {rate:.1f} packets/sec | Valid: {parser.valid_packets}")
                else:
                    print(f"\n[STATS] No valid packets yet...")
                last_stats_time = time.time()
            
            time.sleep(0.001)  # Small delay to prevent CPU spinning
        
        ser.close()
        print(f"\n[SUCCESS] Serial port closed")
        
        # Final statistics
        parser.print_stats()
        
        # Calculate data rate
        elapsed = time.time() - start_time
        if parser.valid_packets > 0:
            rate = parser.valid_packets / elapsed
            expected_rate = 25 # Usually 500 for ESP32, lowered because of default baud rate
            print(f"\n[PERFORMANCE] Performance at {baud_rate} baud:")
            print(f"   Actual rate:   {rate:.1f} packets/sec")
            print(f"   Expected rate: {expected_rate} packets/sec")
            print(f"   Efficiency:    {rate/expected_rate*100:.1f}%")
            
            if rate > expected_rate * 0.7:
                print("   [SUCCESS] Good packet reception at 9600 baud!")
            elif rate > expected_rate * 0.3:
                print("   [WARNING] Moderate packet loss at 9600 baud")
            else:
                print("   [ERROR] High packet loss - check connection")
        else:
            print(f"\n[ERROR] No valid packets received!")
            print(f"[INFO] This suggests the board is not sending data")
        
        return parser.valid_packets > 0
        
    except serial.SerialException as e:
        print(f"[ERROR] Serial error: {e}")
        return False
    except KeyboardInterrupt:
        print(f"\n[STOP] Test interrupted by user")
        return False
    except Exception as e:
        print(f"[ERROR] Unexpected error: {e}")
        return False

def test_port_detection():
    """Test if the port can be detected and opened"""
    import serial.tools.list_ports
    
    print("[SCAN] Scanning for serial ports...")
    ports = list(serial.tools.list_ports.comports())
    
    if not ports:
        print("[ERROR] No serial ports found")
        return False
    
    print("[PORTS] Available ports:")
    target_port = None
    for port in ports:
        print(f"   {port.device} - {port.description}")
        if 'usbserial' in port.device:
            target_port = port.device
    
    if target_port:
        print(f"[TARGET] Target port detected: {target_port}")
        return True
    else:
        print("[WARNING] No USB serial ports found")
        return False

if __name__ == "__main__":
    print("[TEST] Cerelog X8 Raw Serial Test")
    print("=" * 40)
    
    # Test 1: Port detection
    if not test_port_detection():
        print("\n[ERROR] Port detection failed")
        sys.exit(1)
    
    print("")
    
    # Test 2: Serial communication
    if test_serial_connection():
        print("\n[SUCCESS] Serial test completed successfully!")
        print("   Your ESP32 is sending properly formatted packets.")
        sys.exit(0)
    else:
        print("\n[ERROR] Serial test failed")
        print("   Check ESP32 firmware, connections, or port settings.")
        sys.exit(1)
