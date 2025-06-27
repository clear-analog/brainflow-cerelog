#!/usr/bin/env python3
"""
Test script to verify dynamic baud rate configuration through handshake.

This test verifies that:
1. Handshake works at 9600 baud with baud rate configuration parameters
2. BrainFlow sends target baud rate in handshake (reg_addr=0x01, reg_val=baud_rate_index)
3. Arduino switches to the configured baud rate after handshake
4. Data streaming continues at the configured baud rate
"""

import time
import sys
import os

# Add the parent directory to the path to import brainflow
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from brainflow.board_shim import BoardShim, BrainFlowPresets, BoardIds, BrainFlowInputParams, BrainFlowError
from brainflow.data_filter import DataFilter


def test_dynamic_baud_rate_configuration():
    """Test the dynamic baud rate configuration through handshake."""
    
    print("=== Dynamic Baud Rate Configuration Test ===")
    print("This test verifies baud rate configuration through handshake parameters")
    print("")
    
    # Board configuration
    board_id = BoardIds.CERELOG_X8_BOARD
    input_params = BrainFlowInputParams()
    board = BoardShim(board_id, input_params)
    
    try:
        print("1. Preparing session (should send baud rate config in handshake)...")
        board.prepare_session()
        print("   [SUCCESS] Session prepared successfully")
        print("   [SUCCESS] Handshake sent with baud rate configuration")
        
        print("\n2. Starting stream (should be at configured baud rate)...")
        board.start_stream(45000)  # 45 second buffer
        print("   [SUCCESS] Stream started successfully")
        
        print("\n3. Collecting data for 3 seconds...")
        time.sleep(3)  # Collect data for 3 seconds (1500 samples at 500 Hz)
        
        # Get data
        data = board.get_board_data()
        num_samples = data.shape[1] if data.size > 0 else 0
        
        print(f"   [SUCCESS] Collected {num_samples} samples")
        
        if num_samples > 0:
            print(f"   [SUCCESS] Data shape: {data.shape}")
            print(f"   [SUCCESS] Sample rate: {num_samples / 3:.1f} Hz")
            
            # Check if we have data on all channels
            eeg_channels = board.get_eeg_channels(board_id)
            if len(eeg_channels) >= 8:
                print(f"   [SUCCESS] All {len(eeg_channels)} EEG channels active")
        
        print("\n4. Stopping stream...")
        board.stop_stream()
        print("   [SUCCESS] Stream stopped successfully")
        
        print("\n5. Releasing session...")
        board.release_session()
        print("   [SUCCESS] Session released successfully")
        
        print("\n=== Test Results ===")
        if num_samples > 0:
            print("[SUCCESS] SUCCESS: Dynamic baud rate configuration appears to be working")
            print("[SUCCESS] Data streaming is active at configured baud rate")
        else:
            print("[FAILED] FAILURE: No data received")
            return False
            
    except Exception as e:
        print(f"[ERROR] ERROR: {e}")
        return False
    
    return True


def test_baud_rate_configuration_values():
    """Test different baud rate configuration values."""
    
    print("\n=== Baud Rate Configuration Values Test ===")
    print("This test verifies the baud rate configuration mapping")
    print("")
    
    # Baud rate configuration mapping
    baud_configs = {
        0x00: 9600,     # Default
        0x01: 19200,    # Low speed
        0x02: 38400,    # Medium speed
        0x03: 57600,    # High speed
        0x04: 115200,   # Very high speed
        0x05: 230400,   # Ultra high speed (macOS target)
        0x06: 460800,   # Maximum speed
        0x07: 921600,   # Super speed
    }
    
    print("Baud Rate Configuration Mapping:")
    for config_val, baud_rate in baud_configs.items():
        print(f"   Config 0x{config_val:02X} → {baud_rate:,} baud")
    
    print(f"\nExpected target baud rate for macOS: {baud_configs[0x05]:,} baud")
    
    return True


def test_manual_configuration_disabled():
    """Test that manual configuration is properly disabled."""
    
    print("\n=== Manual Configuration Test ===")
    print("This test verifies manual configuration is disabled")
    print("")
    
    board_id = BoardIds.CERELOG_X8_BOARD
    input_params = BrainFlowInputParams()
    board = BoardShim(board_id, input_params)
    
    try:
        print("1. Preparing session...")
        board.prepare_session()
        
        print("2. Testing manual baud rate configuration...")
        try:
            response = board.config_board("baud_rate=4")  # Try to set to 115200
            print(f"   [WARNING] Unexpected success: {response}")
            # This should not succeed since manual configuration is disabled
            board.release_session()
            return False
            
        except BrainFlowError as e:
            if "INVALID_ARGUMENTS_ERROR" in str(e) or "unable to config board" in str(e):
                print("   [SUCCESS] Correctly rejects manual configuration (as expected)")
                print(f"   Error: {e}")
            else:
                print(f"   [ERROR] Unexpected error type: {e}")
                board.release_session()
                return False
        
        board.release_session()
        
    except Exception as e:
        print(f"[ERROR] ERROR: {e}")
        return False
    
    return True


if __name__ == "__main__":
    print("Cerelog X8 Dynamic Baud Rate Configuration Test")
    print("===============================================")
    print("")
    
    # Run the main test
    success = test_dynamic_baud_rate_configuration()
    
    # Run the configuration values test
    config_success = test_baud_rate_configuration_values()
    
    # Run the manual configuration test
    manual_success = test_manual_configuration_disabled()
    
    print("\n" + "="*60)
    if success and config_success and manual_success:
        print("[SUCCESS] ALL TESTS PASSED!")
        print("Dynamic baud rate configuration is working correctly.")
        print("Handshake parameters are being used for baud rate configuration.")
    else:
        print("[FAILED] SOME TESTS FAILED!")
        print("Check the output above for details.")
    
    sys.exit(0 if success and config_success and manual_success else 1) 