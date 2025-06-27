#!/usr/bin/env python3
"""
Comprehensive Test Runner for Cerelog X8 Integration
Runs all tests and provides a summary report
"""

import subprocess
import sys
import time
import os
import warnings
from datetime import datetime

# Suppress deprecation warnings
warnings.filterwarnings("ignore", category=DeprecationWarning, module="pkg_resources")

def run_test(test_name, test_file, description):
    """Run a single test and return results"""
    print(f"\n{'='*60}")
    print(f"[TEST] Running: {test_name}")
    print(f"[INFO] Description: {description}")
    print(f"{'='*60}")
    
    start_time = time.time()
    try:
        result = subprocess.run([sys.executable, test_file], 
                              capture_output=True, text=True, timeout=60)
        duration = time.time() - start_time
        
        # Check if stderr contains only deprecation warnings
        stderr_clean = result.stderr
        if stderr_clean:
            # Remove deprecation warnings from stderr
            lines = stderr_clean.split('\n')
            filtered_lines = []
            for line in lines:
                if 'UserWarning' not in line and 'pkg_resources' not in line and 'deprecated' not in line.lower():
                    filtered_lines.append(line)
            stderr_clean = '\n'.join(filtered_lines)
        
        if result.returncode == 0:
            print(f"[SUCCESS] {test_name} PASSED ({duration:.1f}s)")
            return True, result.stdout, stderr_clean
        else:
            print(f"[FAILED] {test_name} FAILED ({duration:.1f}s)")
            if stderr_clean:
                print(f"Error: {stderr_clean}")
            return False, result.stdout, stderr_clean
            
    except subprocess.TimeoutExpired:
        print(f"[TIMEOUT] {test_name} TIMEOUT (>60s)")
        return False, "", "Timeout"
    except Exception as e:
        print(f"[ERROR] {test_name} ERROR: {e}")
        return False, "", str(e)

def main():
    """Run all tests and generate summary"""
    print("[START] Cerelog X8 Integration Test Suite")
    print(f"[TIME] Started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    tests = [
        {
            "name": "Raw Serial Test",
            "file": "test_serial.py",
            "description": "Tests direct serial communication (bypasses BrainFlow)"
        },
        {
            "name": "Handshake Test",
            "file": "test_handshake.py",
            "description": "Tests handshake protocol, baud rate switching, and ring buffer implementation"
        },
        {
            "name": "BrainFlow Integration Test",
            "file": "test_brainflow.py",
            "description": "Tests basic BrainFlow integration, handshake, and data streaming"
        },
        {
            "name": "EEG Validation Test",
            "file": "test_validate_eeg.py", 
            "description": "Tests EEG signal quality, RMS, voltage ranges, and CMRR"
        },
        {
            "name": "Unix Timestamp Test", 
            "file": "test_unix_timestamps.py",
            "description": "Tests timestamp synchronization between Arduino and BrainFlow"
        },
        {
            "name": "Baud Rate Switch Test",
            "file": "test_baud_rate_switch.py",
            "description": "Tests dynamic baud rate configuration and handshake switching"
        }
    ]
    
    results = []
    passed = 0
    failed = 0
    
    for test in tests:
        if os.path.exists(test["file"]):
            success, stdout, stderr = run_test(test["name"], test["file"], test["description"])
            results.append({
                "name": test["name"],
                "success": success,
                "stdout": stdout,
                "stderr": stderr
            })
            if success:
                passed += 1
            else:
                failed += 1
        else:
            print(f"[WARNING] {test['name']}: File not found ({test['file']})")
            failed += 1
    
    # Summary report
    print(f"\n{'='*60}")
    print("[SUMMARY] TEST SUMMARY")
    print(f"{'='*60}")
    print(f"[SUCCESS] Passed: {passed}")
    print(f"[FAILED] Failed: {failed}")
    print(f"[STATS] Success Rate: {passed/(passed+failed)*100:.1f}%")
    print(f"[TIME] Completed at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    if failed > 0:
        print(f"\n[FAILED] FAILED TESTS:")
        for result in results:
            if not result["success"]:
                print(f"  - {result['name']}")
                if result["stderr"]:
                    print(f"    Error: {result['stderr'][:100]}...")
    
    print(f"\n[INFO] RECOMMENDATIONS:")
    if passed == len(tests):
        print("  [SUCCESS] All tests passed! Your Cerelog X8 integration is working correctly.")
    elif passed >= len(tests) - 1:
        print("  [SUCCESS] Most tests passed. Check the failed test for specific issues.")
    else:
        print("  [WARNING] Multiple tests failed. Review the integration setup.")
    
    print(f"\n[FILES] Log files generated:")
    log_files = [
        "test_serial.log", 
        "test_handshake.log",
        "test_brainflow.log", 
        "test_unix_timestamps.log", 
        "test_validate_eeg.log",
        "test_baud_rate_switch.log"
    ]
    for log_file in log_files:
        if os.path.exists(log_file):
            size = os.path.getsize(log_file) / 1024  # KB
            print(f"  - {log_file} ({size:.1f} KB)")
        else:
            print(f"  - {log_file} (not generated)")
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main()) 