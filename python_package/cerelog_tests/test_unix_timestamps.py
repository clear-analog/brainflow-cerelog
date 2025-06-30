#!/usr/bin/env python3
"""
Test Unix timestamp functionality for Cerelog X8
This test shows the difference between incremental and Unix timestamps
"""

import sys
import os
import time
import platform
import re
from datetime import datetime

# Add the parent directory to the path to import Brainflow
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from brainflow.board_shim import BoardShim, BrainFlowInputParams, BoardIds, LogLevels

def analyze_timestamp_logs(log_file):
    """Parse log file for timestamp debugging information"""
    
    if not os.path.exists(log_file):
        print(f"[ERROR] Log file '{log_file}' not found")
        return
    
    print(f"[INFO] Analyzing timestamp logs from: {log_file}")
    print("=" * 60)
    
    timestamp_patterns = [
        r"Packet #(\d+): board_timestamp=([\d.]+), system_time=([\d.]+), diff=([-\d.]+)s",
        r"Using scanned port for (\w+): (.+)",
        r"Found available port: (.+)",
        r"Sample #(\d+).*ch(\d+)=([\d.-]+).*ch(\d+)=([\d.-]+)"  # Timestamp channel data (RegEx)
    ]
    
    found_timestamps = []
    found_ports = []
    found_samples = []
    
    try:
        with open(log_file, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                
                # Look for timestamp debugging messages
                for pattern in timestamp_patterns:
                    match = re.search(pattern, line)
                    if match:
                        if "board_timestamp" in line:
                            packet_num = int(match.group(1))
                            board_ts = float(match.group(2))
                            system_ts = float(match.group(3))
                            diff = float(match.group(4))
                            
                            found_timestamps.append({
                                'packet': packet_num,
                                'board_ts': board_ts,
                                'system_ts': system_ts,
                                'diff': diff,
                                'line': line_num
                            })
                        elif "scanned port" in line or "Found available port" in line:
                            found_ports.append({
                                'message': line,
                                'line': line_num
                            })
                        elif "Sample #" in line and "ch" in line:
                            found_samples.append({
                                'message': line,
                                'line': line_num
                            })
    
        # Display results
        if found_timestamps:
            print("[DEBUG] TIMESTAMP DEBUGGING INFO:")
            print("-" * 40)
            for ts_info in found_timestamps:
                board_dt = datetime.fromtimestamp(ts_info['board_ts'])
                system_dt = datetime.fromtimestamp(ts_info['system_ts'])
                
                print(f"Packet {ts_info['packet']}:")
                print(f"  Board:    {ts_info['board_ts']:.2f} -> {board_dt.strftime('%H:%M:%S')}")
                print(f"  System:   {ts_info['system_ts']:.2f} -> {system_dt.strftime('%H:%M:%S')}")
                print(f"  Diff:     {ts_info['diff']:.2f}s\n")
        
        if found_ports:
            print("[PORT] PORT DETECTION INFO:")
            print("-" * 40)
            for port_info in found_ports:
                print(f"Line {port_info['line']}: {port_info['message']}")
            print("")
        
        if found_samples:
            print("[DATA] SAMPLE DATA (most recent 3):")
            print("-" * 40)
            for sample_info in found_samples[-3:]:  # Show last 3 instead of first 3
                print(f"Line {sample_info['line']}: {sample_info['message']}")
            print("")
        
        if not found_timestamps and not found_ports and not found_samples:
            print("[ERROR] No timestamp debugging information found in log")
            print("   Make sure to run the test with debug logging enabled")
    
    except Exception as e:
        print(f"[ERROR] Error reading log file: {e}")

def test_unix_timestamps():
    """Test Unix timestamp functionality"""
    params = BrainFlowInputParams()
    # Port scanning will automatically detect the correct port
    
    print(f"[TEST] Testing Unix Timestamps on {platform.system()} (will auto-detect port)")
    print("=" * 60)
    
    params.timeout = 3
    time_len = 5  # seconds - shorter test to see timestamps
    log_file = 'test_unix_timestamps.log'
    
    try:
        board = BoardShim(BoardIds.CERELOG_X8_BOARD, params)
        BoardShim.enable_dev_board_logger()
        BoardShim.set_log_level(LogLevels.LEVEL_DEBUG.value)
        BoardShim.set_log_file(log_file)
        
        sample_rate = BoardShim.get_sampling_rate(BoardIds.CERELOG_X8_BOARD)
        eeg_channels = BoardShim.get_eeg_channels(BoardIds.CERELOG_X8_BOARD)
        timestamp_channel = BoardShim.get_timestamp_channel(BoardIds.CERELOG_X8_BOARD)
        
        print(f"[STATS] Sample rate: {sample_rate} SPS")
        print(f"[STATS] EEG channels: {eeg_channels}")
        print(f"[STATS] Timestamp channel: {timestamp_channel}")
        print("")

        board.prepare_session()
        print("[SUCCESS] Session prepared successfully")
        
        board.start_stream()
        print("[SUCCESS] Stream started successfully")
        
        # Collect data for specified duration
        print(f"[TIME] Collecting data for {time_len} seconds...")
        time.sleep(time_len)  # Collect some data
        
        board.stop_stream()
        print("[SUCCESS] Stream stopped successfully")
        
        # Get the data
        data = board.get_board_data()
        board.release_session()
        
        print(f"[DATA] Collected {data.shape[1]} samples")
        print("")
        
        # Analyze timestamps
        if timestamp_channel >= 0 and timestamp_channel < data.shape[0]:
            timestamps = data[timestamp_channel, :]
            
            print("[TIMESTAMP] TIMESTAMP ANALYSIS:")
            print("-" * 40)
            
            # Show first few timestamps
            print("First 5 timestamps:")
            for i in range(min(5, len(timestamps))):
                timestamp = timestamps[i]
                if timestamp > 0:  # Valid timestamp
                    dt = datetime.fromtimestamp(timestamp)
                    print(f"  Sample {i}: {timestamp:.2f} -> {dt.strftime('%Y-%m-%d %H:%M:%S')}")
                else:
                    print(f"  Sample {i}: {timestamp:.2f} (invalid)")
            
            print("")
            
            # Show timestamp progression
            valid_timestamps = [t for t in timestamps if t > 0]
            if len(valid_timestamps) >= 2:
                first_ts = valid_timestamps[0]
                last_ts = valid_timestamps[-1]
                duration = last_ts - first_ts
                
                print(f"[STATS] Timestamp Statistics:")
                print(f"  First timestamp: {first_ts:.2f} ({datetime.fromtimestamp(first_ts).strftime('%H:%M:%S')})")
                print(f"  Last timestamp:  {last_ts:.2f} ({datetime.fromtimestamp(last_ts).strftime('%H:%M:%S')})")
                print(f"  Duration:        {duration:.2f} seconds")
                print(f"  Expected:        {time_len} seconds")
                print(f"  Difference:      {abs(duration - time_len):.2f} seconds")
                
                # Check if timestamps are incrementing (old) or Unix (new)
                if first_ts > 1700000000:  # If first timestamp is a large Unix timestamp (after 2023)
                    print(f"  [SUCCESS] Detected: Unix timestamps (real time)")
                    print(f"  [INFO] Format: Unix epoch seconds (e.g., {first_ts:.0f} = {datetime.fromtimestamp(first_ts).strftime('%Y-%m-%d %H:%M:%S')})")
                else:
                    print(f"  [WARNING] Detected: Incrementing timestamps (counter)")
                    print(f"  [INFO] Format: Packet counter (e.g., {first_ts:.0f} = packet #{first_ts:.0f})")
                
                print("")
                print("[COMPARE] COMPARISON:")
                print("-" * 40)
                print("OLD (Incrementing): 0, 1, 2, 3, 4, 5... (packet counter)")
                print("NEW (Unix):        1703123456, 1703123457, 1703123458... (real time)")
                print("")
                if first_ts > 1700000000:
                    print("[SUCCESS] SUCCESS: Unix timestamps are working!")
                    print("   - Timestamps represent real time")
                    print("   - Easy to correlate with system logs")
                    print("   - No complex synchronization needed")
                else:
                    print("[WARNING] Still using old incrementing timestamps")
                    print("   - Check if new firmware is uploaded")
                    print("   - Check if BrainFlow is rebuilt")
            
            print("")
            
            # Show system time comparison
            system_time = time.time()
            print(f"[SYSTEM] System time: {system_time:.2f} ({datetime.fromtimestamp(system_time).strftime('%H:%M:%S')})")
            
            if len(valid_timestamps) > 0:
                latest_board_time = valid_timestamps[-1]
                time_diff = system_time - latest_board_time
                print(f"[TIME] Time difference (system - board): {time_diff:.2f} seconds")
                
                if abs(time_diff) < 10:
                    print("[SUCCESS] Timestamps are well synchronized!")
                elif abs(time_diff) < 60:
                    print("[WARNING] Timestamps are roughly synchronized (within 1 minute)")
                else:
                    print("[ERROR] Timestamps are not well synchronized")
        
        else:
            print("[ERROR] No timestamp channel found in data")
        
        print("")
        print("[LOG] Analyzing log file for detailed debugging...")
        print("")
        
        # Analyze the log file
        analyze_timestamp_logs(log_file)
        
    except Exception as e:
        print(f"[ERROR] Error: {e}")
        return False
    
    return True

if __name__ == "__main__":
    print("[TEST] Cerelog X8 Unix Timestamp Test")
    print("=" * 40)
    print("")
    
    success = test_unix_timestamps()
    
    if success:
        print("[SUCCESS] Test completed successfully!")
    else:
        print("[ERROR] Test failed!")
        sys.exit(1) 
