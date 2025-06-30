# Cerelog X8 Integration Test Suite

This directory contains comprehensive tests for the Cerelog X8 EEG board integration with BrainFlow.

## 🚀 Quick Start

Run all tests with one command:
```bash
python3 run_all_tests.py
```

## 📋 Test Overview

### 1. **test_serial.py** - Raw Serial Communication
- **Purpose**: Tests direct serial communication (bypasses BrainFlow)
- **What it tests**:
  - Direct packet parsing
  - Checksum validation
  - Packet statistics
- **Note**: This is a lower-level test for debugging serial issues

### 2. **test_handshake.py** - Handshake Protocol Test
- **Purpose**: Tests handshake protocol, baud rate switching, and ring buffer implementation
- **What it tests**:
  - New handshake packet format with markers `0xAA 0xBB` and `0xCC 0xDD`
  - Ring buffer implementation for robust handshake detection
  - Baud rate switching from 9600 to 230400 baud
  - Data streaming after handshake
- **Key Feature**: 24-byte ring buffer eliminates timing dependencies

### 3. **test_brainflow.py** - Main Integration Test
- **Purpose**: Tests basic BrainFlow integration, handshake, and data streaming
- **What it tests**:
  - Port detection and connection
  - Timestamp handshake protocol
  - Data streaming at 500 SPS
  - EEG channel data quality
  - RMS calculations for all 8 channels
- **Output**: CSV file with raw data, console output with channel statistics
- **Debug**: Shows raw values for channels 2 and 8 to help diagnose issues

### 4. **test_unix_timestamps.py** - Timestamp Synchronization
- **Purpose**: Tests timestamp synchronization between Arduino and BrainFlow
- **What it tests**:
  - Unix timestamp handshake
  - Time synchronization accuracy
  - Timestamp progression over time
- **Output**: Detailed timestamp analysis and comparison

### 5. **test_validate_eeg.py** - Signal Quality Validation
- **Purpose**: Tests EEG signal quality and validates data integrity
- **What it tests**:
  - RMS values for each channel
  - Voltage range validation
  - Common Mode Rejection Ratio (CMRR)
  - Signal quality metrics
- **Output**: Validation results saved to CSV

### 6. **test_baud_rate_switch.py** - Baud Rate Configuration
- **Purpose**: Tests dynamic baud rate configuration and handshake switching
- **What it tests**:
  - Baud rate switching functionality
  - Configuration parameter passing
  - Dynamic reconfiguration

## 🔧 Configuration

### Baud Rate
- **Arduino**: Default baud rate is 9600
- **BrainFlow**: Automatically re-configures baud per operating system

### Port Detection
All tests use automatic port scanning:
- **Windows**: COM1-COM20
- **macOS**: `/dev/cu.usbserial-*` patterns
- **Linux**: `/dev/ttyUSB*` and `/dev/ttyACM*` patterns

### Handshake Protocol
The timestamp handshake uses a 12-byte packet format with new markers:
```
[0xAA][0xBB][0x02][timestamp][timestamp][timestamp][timestamp][reg_addr][reg_val][checksum][0xCC][0xDD]
```

## 📊 Expected Results

### Successful Integration
- ✅ Handshake successful with ring buffer
- ✅ Streaming starts without timeout
- ✅ 8 EEG channels receiving data
- ✅ RMS values > 0.001V for active channels
- ✅ Timestamp synchronization within ±1 second
- ✅ Fast startup (< 10 seconds)

## 🐛 Debugging

### Log Files
Each test generates detailed logs:
- `test_brainflow.log` - Main integration logs
- `test_handshake.log` - Handshake and ring buffer debugging
- `test_unix_timestamps.log` - Timestamp debugging
- `test_validate_eeg.log` - Signal validation logs

### Verbose Logging
BrainFlow debug logging is enabled by default. To reduce verbosity, modify the log level in each test:
```python
BoardShim.set_log_level(LogLevels.LEVEL_INFO.value)  # Less verbose
```

## 📈 Performance Metrics

### Expected Performance
- **Sample Rate**: 500 SPS
- **Data Rate**: ~18.5 KB/s (37 bytes × 500 Hz)
- **Latency**: < 10ms from sensor to BrainFlow
- **Packet Loss**: < 0.1%

### Validation Thresholds
- **RMS Range**: 0.1 - 50 µV
- **Voltage Range**: -100 to +100 µV
- **CMRR**: ≥ 60 dB
- **Timestamp Sync**: ±1 second
- **Startup Time**: < 10 seconds

## 📝 Usage Examples

### Run Individual Tests
```bash
# Raw serial test
python3 test_serial.py

# Handshake test
python3 test_handshake.py

# Main integration test
python3 test_brainflow.py

# Timestamp test
python3 test_unix_timestamps.py

# Signal validation
python3 test_validate_eeg.py

# Baud rate test
python3 test_baud_rate_switch.py
```

### Run All Tests
```bash
python3 run_all_tests.py
```

### Check Results
```bash
# View generated data
cat data.csv

# Check validation results
cat validation_results.csv

# View logs
tail -50 test_brainflow.log
```

## 📞 Support

For issues with the test suite:
1. Check the log files for detailed error messages
2. Verify Arduino firmware is up to date
3. Ensure correct baud rate (9600 initial, variable after handshake)
4. Power cycle Arduino device
5. Check ring buffer implementation in firmware