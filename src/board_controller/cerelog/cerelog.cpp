/**********************
Authors: 1. Suraj Kumar (surajratnam99@gmail.com)
         2. Ben Beaudet (bbeaudet0@gmail.com)
***********************/

#include "cerelog.h"
#include "os_serial.h" // replaced FTDI
#include "serial.h"    // OSSerial needs Serial class to compile properly
#include <ctime>
#include <stdint.h>

#ifndef _WIN32
#include <errno.h>
#endif

// serial port & baud rate conditionals
struct PortInfo
{
    std::string os;
    int baudrate;
    int default_baudrate;
};
PortInfo get_port_info ()
{
    PortInfo info;
    info.default_baudrate = 9600;
// OS detection
#ifdef _WIN32
    info.os = "Windows";
    info.baudrate = 921600;
#elif defined(__APPLE__)
    info.os = "Darwin";   // MacOS
    info.baudrate = 230400;
#elif defined(__linux__)
    info.os = "Linux";
    info.baudrate = 921600; // TODO needs verification
#else
    info.os = "Unknown";
    info.baudrate = 115200; // TODO set to lowest common denominator OR prompt manual input
#endif
    return info;
}

constexpr int LOG_FREQUENCY = 1000; // Log every 1000 samples

/* Constructor */
Cerelog_X8::Cerelog_X8 (int board_id, struct BrainFlowInputParams params) : Board (board_id, params)
{
    serial = NULL;
    is_streaming = false;
    keep_alive = false;
    initialized = false;
    sync_established = false;
    first_packet_counter = 0;
    first_packet_timestamp = 0.0;
    last_sync_counter = 0;
    last_sync_timestamp = 0.0;
    state = (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR;

    // TODO can this section be deleted?
    /* Set sampling_rate from params
    if (params.other_info.empty()) {
        sampling_rate = 500.0; // default if not provided
    } else {
        try {
            sampling_rate = std::stod(params.other_info);
        } catch (...) {
            sampling_rate = 500.0;
        }
    } */
}


/* Configures serial port and baud rate that Cerelog X8 will use */
int Cerelog_X8::prepare_session ()
{
    // PORT SCANNING: Automatically detect available serial ports for the device
    std::string port_path;
    auto info = get_port_info ();
    if (params.serial_port.empty ())
    {
        port_path = scan_for_device_port (); // Use port scanning instead of default
        safe_logger (spdlog::level::info, "Using scanned port for {} OS: {}", info.os, port_path);
    }
    else
    {
        port_path = params.serial_port;
        safe_logger (spdlog::level::info, "Using user-specified port: {}", port_path);
    }

    // Open serial port - create OSSerial directly
    serial = new OSSerial (port_path.c_str ());
    // Old code with FTDI: serial = Serial::create(port_path.c_str(), this);
    int response = serial->open_serial_port ();
    if (response < 0)
    {
        safe_logger (spdlog::level::err, "Failed to open serial port: {}", port_path);
        return (int)BrainFlowExitCodes::UNABLE_TO_OPEN_PORT_ERROR;
    }

    // Set default baud rate for handshake
    response = serial->set_custom_baudrate (info.default_baudrate);
    if (response < 0)
    {
        safe_logger (
            spdlog::level::err, "Failed to set default baudrate: {}", info.default_baudrate);
        return (int)BrainFlowExitCodes::BOARD_WRITE_ERROR;
    }

    // Set other serial settings
    response = serial->set_serial_port_settings (
        params.timeout * 3000, false); // timeout (params.timeout times 3 seconds)
    if (response < 0)
    {
        safe_logger (spdlog::level::err, "Timed out - failed to set serial port settings");
        return (int)BrainFlowExitCodes::BOARD_WRITE_ERROR;
    }

    // Send timestamp handshake to board with baud rate configuration
    // reg_addr = 0x01 for baud rate configuration, reg_val = baud rate index
    uint8_t baud_config = 0x04; // 115200
    if (info.baudrate == 230400) baud_config = 0x05;      // 230400
    else if (info.baudrate == 460800) baud_config = 0x06; // 460800
    else if (info.baudrate == 921600) baud_config = 0x07; // 921600
    
    response = send_timestamp_handshake (0x01, baud_config);
    if (response != (int)BrainFlowExitCodes::STATUS_OK)
    {
        safe_logger (
            spdlog::level::warn, "Timestamp handshake failed, continuing with fallback time");
    }

    // Switch to target baud rate for data streaming
    safe_logger (spdlog::level::info, "Switching to target baud rate: {}", info.baudrate);
    response = serial->set_custom_baudrate (info.baudrate);
    if (response < 0)
    {
        safe_logger (
            spdlog::level::warn, "Failed to switch to target baudrate: {}, continuing with default: {}", info.baudrate, info.default_baudrate);
    } else {
        safe_logger (spdlog::level::info, "Successfully switched to target baud rate: {} on OS: {}", info.baudrate, info.os);
        
        // Flush serial buffer to clear partial packets after baud rate switch
        std::this_thread::sleep_for (std::chrono::milliseconds (300)); // 0.3 seconds
        unsigned char flush_buffer[1000];
        int flush_result = serial->read_from_serial_port (flush_buffer, sizeof(flush_buffer));
        if (flush_result > 0) {
            safe_logger (spdlog::level::debug, "Flushed {} bytes from serial buffer after baud rate switch", flush_result);
        }
    }

    // Give Arduino time to reset after serial connection
    std::this_thread::sleep_for (std::chrono::milliseconds (300)); // 0.3 seconds
    
    // Additional delay after handshake to ensure ESP32 is ready
    std::this_thread::sleep_for (std::chrono::milliseconds (500)); // 0.5 seconds after handshake

    // Successfully prepared session
    initialized = true;
    return (int)BrainFlowExitCodes::STATUS_OK;
}


/* This sends config bytes to the message server on Cerelog X8 and it then packages and executes
 * them */
int Cerelog_X8::config_board (std::string config, std::string &response)
{
    // Simple baud rateconfiguration
    response = "Configuration not supported in current implementation. Using automatic baud rate switching.";
    return (int)BrainFlowExitCodes::INVALID_ARGUMENTS_ERROR;
}


/* TODO Notes
    This function seems a bit unnecessary for Cerelog since we start barraging with data anyway
    This function is also going to enable timestamp syncing?
    This function CALLS READ_THREAD
*/
int Cerelog_X8::start_stream (int buffer_size, const char *streamer_params)
{
    if (!initialized)
    {
        return (int)BrainFlowExitCodes::BOARD_NOT_CREATED_ERROR;
    }

    if (is_streaming)
    {
        return (int)BrainFlowExitCodes::STREAM_ALREADY_RUN_ERROR;
    }

    int res = prepare_for_acquisition (buffer_size, streamer_params); // this is BrainFlow command
    if (res != (int)BrainFlowExitCodes::STATUS_OK)
    {
        safe_logger(spdlog::level::debug, "Stuck at prepare_for_acquisition()");
        return res;
    }
    // Give Arduino time to reset after serial connection
    std::this_thread::sleep_for (std::chrono::milliseconds (300)); // 0.3 seconds
    
    // Streaming begins now - firmware already started streaming after handshake
    safe_logger (spdlog::level::debug, "Starting streaming - (firmware automatically started streaming after handshake)");
    // No need to send "b\n" command - firmware starts streaming immediately after handshake
    keep_alive = true;
    streaming_thread = std::thread ([this] { this->read_thread (); });

    // Check for incoming data, set timeout
    safe_logger (spdlog::level::debug, "Checking for incoming data using mutex");
    std::unique_lock<std::mutex> lk (this->m); // TODO What is mutex?
    auto sec = std::chrono::milliseconds (10000); // 10 seconds
    bool state_changed = cv.wait_for (lk, sec,
        [this] () {
            if (this->state == (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR)
            {
                safe_logger (spdlog::level::warn, "SYNC_TIMEOUT_ERROR detected in wait_for lambda");
            }
            return (this->state != (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR);
        });

    if (state_changed) { // how is state_changed being calculated?
        this->is_streaming = true;
        safe_logger (spdlog::level::debug,
            "The state of the board has changed from TIMEOUT ERROR to " +
                std::to_string (this->state));
        return this->state;
    }
    else
    {
        // Timeout occurred - clean up and return error
        safe_logger (spdlog::level::warn, "Board timed out - stopping thread and cleaning up");
        this->keep_alive = false; // Stop the read thread
        if (streaming_thread.joinable ())
        {
            streaming_thread.join (); // Wait for thread to finish
        }
        this->is_streaming = false; // Ensure streaming flag is false
        return (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR;
    }
}


/* Function sends the current system time through a handshake with optional configuration */
int Cerelog_X8::send_timestamp_handshake (uint8_t reg_addr, uint8_t reg_val)
{
    // Get system time or set fallback
    std::time_t current_time = std::time (nullptr);
    if (current_time < 1600000000)
    {
        current_time = 1500000000; // if system time is before ~2020, use July 2017 as fallback
        safe_logger (
            spdlog::level::warn, "System clock appears incorrect, using fallback timestamp");
    }
    uint32_t unix_timestamp = static_cast<uint32_t> (current_time);

    // Build timestamp packet
    // [start_marker][msg_type][timestamp][timestamp][timestamp][timestamp][RegAddr][RegVal][checksum][end_marker]
    unsigned char packet[12];
    packet[0] = 0xAA;                        // start marker byte 1
    packet[1] = 0xBB;                        // start marker byte 2
    packet[2] = 0x02;                        // message type
    packet[3] = unix_timestamp >> 24 & 0xFF; // timestamp MSB
    packet[4] = unix_timestamp >> 16 & 0xFF; // timestamp second byte
    packet[5] = unix_timestamp >> 8 & 0xFF;  // timestamp third byte
    packet[6] = unix_timestamp & 0xFF;       // timestamp LSB
    packet[7] = reg_addr;                    // configuration register address
    packet[8] = reg_val;                     // configuration register value
    packet[9] = packet[2] + packet[3] + packet[4] + packet[5] + packet[6] + packet[7] + packet[8]; // checksum
    packet[10] = 0xCC;                       // end marker byte 1
    packet[11] = 0xDD;                       // end marker byte 2

    // DEBUG: print handshake packet bytes
    std::string packet_hex;
    for (int i = 0; i < 12; ++i) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02X ", packet[i]);
        packet_hex += buf;
    }
    safe_logger(spdlog::level::info, "Sending handshake packet: {}", packet_hex);

    // Send packet and wait for "OK" response
    int result = serial->send_to_serial_port (reinterpret_cast<const char *> (packet), 12);
    if (result < 0)
    {
        safe_logger (spdlog::level::err, "Failed to send timestamp packet");
        return (int)BrainFlowExitCodes::BOARD_WRITE_ERROR;
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (5000)); // wait 5 seconds for handshake
    
    // Wait for OK response or any valid data packet
    safe_logger (spdlog::level::debug, "Waiting for handshake response...");
    unsigned char response[50]; // Read more bytes to catch full data packets
    int bytes_read = serial->read_from_serial_port (response, 50); // Read up to 50 bytes
    
    if (bytes_read > 0) {
        safe_logger (spdlog::level::info, "Received handshake response ({} bytes): {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}", 
            bytes_read, response[0], response[1], response[2], response[3], response[4], 
            response[5], response[6], response[7], response[8], response[9]);
        
        // Check if it's a valid data packet (starts with 0xAB 0xCD) anywhere in the response
        for (int i = 0; i < bytes_read - 1; i++) {
            if (response[i] == 0xAB && response[i+1] == 0xCD) {
                safe_logger (spdlog::level::info, "Found valid data packet pattern at position {} - handshake successful!", i);
                return (int)BrainFlowExitCodes::STATUS_OK;
            }
        }
        
        // Check if it's zeros (device not ready yet)
        bool all_zeros = true;
        for (int i = 0; i < bytes_read; i++) {
            if (response[i] != 0x00) {
                all_zeros = false;
                break;
            }
        }
        
        if (all_zeros) {
            safe_logger (spdlog::level::warn, "Received all zeros - device may not be ready yet");
        } else {
            // Accept any non-zero response as handshake success (device is sending data)
            safe_logger (spdlog::level::info, "Received non-zero response from ESP32 - handshake successful!");
            return (int)BrainFlowExitCodes::STATUS_OK;
        }
    } else {
        safe_logger (spdlog::level::err, "No response received from ESP32");
    }
    
    return (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR;
}


/* Function reads the serial data thread */
void Cerelog_X8::read_thread ()
{
    // Constants for packet structure
    constexpr int START_MARKER = 0xABCD;
    constexpr int END_MARKER = 0xDCBA;
    constexpr int PACKET_IDX_START_MARKER = 0;
    constexpr int PACKET_IDX_LENGTH = 2;
    constexpr int PACKET_IDX_TIMESTAMP = 3;
    constexpr int PACKET_IDX_ADS1299_DATA = 7;
    constexpr int ADS1299_TOTAL_DATA_BYTES = 27;
    constexpr int PACKET_IDX_CHECKSUM = PACKET_IDX_ADS1299_DATA + ADS1299_TOTAL_DATA_BYTES;
    constexpr int PACKET_IDX_END_MARKER = PACKET_IDX_CHECKSUM + 1;
    constexpr int PACKET_MSG_LENGTH = 4 + ADS1299_TOTAL_DATA_BYTES; // timestamp + data
    constexpr int PACKET_TOTAL_SIZE =
        2 + 1 + PACKET_MSG_LENGTH + 1 + 2; // start + len + msg + checksum + end

    // Null/validity checks for serial
    if (serial == nullptr)
    {
        safe_logger (spdlog::level::err, "Serial pointer is null in read_thread");
        return;
    }

    // Check board_descr and its keys
    if (board_descr.find ("default") == board_descr.end ())
    {
        safe_logger (spdlog::level::err, "Board descriptor 'default' not found");
        return;
    }
    const auto &default_descr = board_descr["default"];
    if (default_descr.find ("num_rows") == default_descr.end () ||
        default_descr.find ("eeg_channels") == default_descr.end () ||
        default_descr.find ("timestamp_channel") == default_descr.end () ||
        default_descr.find ("marker_channel") == default_descr.end ())
    {
        safe_logger (spdlog::level::err, "Board descriptor missing required fields");
        return;
    }

    int num_rows = 0;
    try
    {
        num_rows = default_descr["num_rows"];
    }
    catch (...)
    {
        safe_logger (spdlog::level::err, "Failed to get num_rows from board_descr");
        return;
    }

    std::vector<int> eeg_channels;
    try
    {
        eeg_channels = default_descr["eeg_channels"].get<std::vector<int>> ();
    }
    catch (...)
    {
        safe_logger (spdlog::level::err, "Failed to get eeg_channels from board_descr");
        return;
    }

    int timestamp_channel = 0;
    int marker_channel = 0;
    try
    {
        timestamp_channel = default_descr["timestamp_channel"];
        marker_channel = default_descr["marker_channel"];
    }
    catch (...)
    {
        safe_logger (spdlog::level::err,
            "Failed to get timestamp_channel or marker_channel from board_descr");
        return;
    }

    // Validate channel indices
    if (timestamp_channel < 0 || timestamp_channel >= num_rows || marker_channel < 0 ||
        marker_channel >= num_rows)
    {
        safe_logger (spdlog::level::err, "Invalid timestamp or marker channel index");
        return;
    }
    // If we don't have enough EEG channels of data coming in
    if (eeg_channels.size () < 8)
    {
        safe_logger (
            spdlog::level::err, "Not enough EEG channels in board_descr (need at least 8)");
        return;
    }

    for (size_t i = 0; i < 8; ++i)
    {
        if (eeg_channels[i] < 0 || eeg_channels[i] >= num_rows)
        {
            safe_logger (spdlog::level::err, "EEG channel index {} out of bounds", eeg_channels[i]);
            return;
        }
    }

    // Use RAII for packet buffer and package buffer
    std::vector<unsigned char> packet (PACKET_TOTAL_SIZE, 0);
    std::vector<double> package (num_rows, 0.0);

    // New implementation: read 37*2 bytes at a time, search for start marker, sync timestamp,
    // parse, push
    constexpr int READ_CHUNK_SIZE = PACKET_TOTAL_SIZE * 2; // 37*2 = 74 bytes
    std::vector<unsigned char> read_buffer (READ_CHUNK_SIZE, 0);
    int buffer_pos = 0;
    int buffer_len = 0;

    // Counter for logging every 1000 samples
    // TODO still needed?
    size_t sample_counter = 0;

    while (keep_alive)
    {
        // Shift any leftover bytes to the start of the buffer
        if (buffer_pos < buffer_len)
        {
            int leftover = buffer_len - buffer_pos;
            if (leftover > 0 && leftover <= READ_CHUNK_SIZE)
            {
                std::memmove (read_buffer.data (), read_buffer.data () + buffer_pos, leftover);
                buffer_len = leftover;
            }
            else
            {
                buffer_len = 0;
            }
        }
        else
        {
            buffer_len = 0;
        }
        buffer_pos = 0;

        // Read more bytes to fill the buffer
        int to_read = READ_CHUNK_SIZE - buffer_len;
        if (to_read <= 0 || (buffer_len < 0) || (buffer_len > READ_CHUNK_SIZE))
        {
            safe_logger (spdlog::level::err, "Buffer length invalid: {}", buffer_len);
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
            continue;
        }

        // TODO Conditionals for serial library, baud rate, sample frequency, etc.?
        int res = serial->read_from_serial_port (read_buffer.data () + buffer_len, to_read);
        if (res > 0)
        {
            buffer_len += res;
            if (buffer_len > READ_CHUNK_SIZE)
            {
                safe_logger (spdlog::level::err, "Buffer overflow detected in read_thread");
                buffer_len = READ_CHUNK_SIZE;
            }
        }
        else
        {
            safe_logger (spdlog::level::debug, "Failed to read from serial port");
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
            continue;
        }

        // Scan for start marker in the buffer
        while (buffer_pos + PACKET_TOTAL_SIZE <= buffer_len)
        {
            // Check for start marker (big endian)
            if (read_buffer[buffer_pos] == ((START_MARKER >> 8) & 0xFF) &&
                read_buffer[buffer_pos + 1] == (START_MARKER & 0xFF))
            {
                if (sample_counter % LOG_FREQUENCY == 0)
                {
                    safe_logger (spdlog::level::debug,
                        "Found potential start marker at position {}", buffer_pos);
                }

                // Check checksum
                uint8_t checksum = 0;
                for (int i = PACKET_IDX_LENGTH; i < PACKET_IDX_CHECKSUM; i++)
                {
                    checksum += read_buffer[buffer_pos + i];
                }
                if (read_buffer[buffer_pos + PACKET_IDX_CHECKSUM] != checksum)
                {
                    safe_logger (spdlog::level::warn, "Checksum mismatch in buffer scan");
                    buffer_pos += 1;
                    continue;
                }

                // Potential packet found, check end marker
                int end_idx = buffer_pos + PACKET_IDX_END_MARKER;
                if (end_idx + 1 >= buffer_len)
                {
                    break; // Not enough data for full packet, break to read more
                }
                if (read_buffer[end_idx] != ((END_MARKER >> 8) & 0xFF) ||
                    read_buffer[end_idx + 1] != (END_MARKER & 0xFF))
                {
                    if (sample_counter % LOG_FREQUENCY == 0)
                    {
                        safe_logger (spdlog::level::warn, "End marker mismatch in buffer scan");
                    }
                    buffer_pos += 1;
                    continue;
                }

                // Copy packet to local buffer for parsing
                std::memcpy (packet.data (), read_buffer.data () + buffer_pos, PACKET_TOTAL_SIZE);

                // Parse timestamp (4 bytes, big endian) - now contains Unix timestamp
                if (PACKET_IDX_TIMESTAMP + 3 >= (int)packet.size ())
                {
                    safe_logger (spdlog::level::err, "Packet too short for timestamp");
                    buffer_pos += 1;
                    continue;
                }
                uint32_t board_timestamp = 0;
                board_timestamp |= (uint32_t)packet[PACKET_IDX_TIMESTAMP] << 24;
                board_timestamp |= (uint32_t)packet[PACKET_IDX_TIMESTAMP + 1] << 16;
                board_timestamp |= (uint32_t)packet[PACKET_IDX_TIMESTAMP + 2] << 8;
                board_timestamp |= (uint32_t)packet[PACKET_IDX_TIMESTAMP + 3];

                // Use board timestamp directly (now a Unix timestamp)
                double synced_timestamp = (double)board_timestamp;

                // Log timestamp info for debugging - fixed-point formatting
                static int timestamp_debug_count = 0;
                if (timestamp_debug_count < 5)
                { // first few packets only
                    double system_time = time (nullptr);
                    safe_logger (spdlog::level::info,
                        "Packet #{}: board_timestamp={:.0f}, system_time={:.0f}, diff={:.0f}s",
                        timestamp_debug_count, synced_timestamp, system_time,
                        system_time - synced_timestamp);
                    timestamp_debug_count++;
                }

                // Parse ADS1299 data (27 bytes, 8 channels, 3 bytes per channel)
                for (int ch = 0; ch < 8; ch++)
                {
                    // added "+3" to skip status bytes
                    int idx =
                        PACKET_IDX_ADS1299_DATA + 3 + ch * 3; // go to start point in the packet,
                    if (idx + 2 >= (int)packet.size ())
                    {
                        safe_logger (spdlog::level::err,
                            "Packet too short for ADS1299 data, channel {}", ch);
                        continue;
                    }

                    // If data looks good
                    int32_t value = ((int32_t)packet[idx] << 16) | ((int32_t)packet[idx + 1] << 8) |
                        ((int32_t)packet[idx + 2]);

                    //  Only log first few samples to avoid spam
                    // TODO delete?
                    static int debug_sample_count = 0;
                    if (sample_counter % LOG_FREQUENCY == 0 && debug_sample_count < 5 && ch == 0)
                    {
                        // Only channel 0, first 5 samples
                        safe_logger (spdlog::level::info,
                            "Sample #{}, Ch0: raw bytes=[0x{:02X} 0x{:02X} 0x{:02X}], "
                            "raw_value={}, after_sign_ext={}",
                            debug_sample_count, packet[idx], packet[idx + 1], packet[idx + 2],
                            value, value);
                        debug_sample_count++;
                    }

                    // apply the mask and check JUST for that bit (sign extension baby)
                    if (value & 0b00000000100000000000000000000000)
                    {
                        value = value | 0b11111111000000000000000000000000;
                    }

                    // More debug for the voltage conversion
                    // TODO remove?
                    if (sample_counter % LOG_FREQUENCY == 0 && debug_sample_count <= 5 && ch == 0)
                    {
                        int gain = 24;
                        float vref = 4.5;
                        double volts = (double)value * ((2.0f * vref / gain) / (1 << 24));
                        safe_logger (
                            spdlog::level::info, "Ch0: final_value={}, volts={}", value, volts);
                    }

                    // Convert to volts and store in correct channel
                    int gain = 24;
                    float vref = 4.5;
                    // double volts = (double)value * (4.5 / (1 << 24));
                    double volts = (double)value * ((2.0f * vref / gain) / (1 << 24));
                    if (eeg_channels[ch] >= 0 && eeg_channels[ch] < num_rows)
                    {
                        package[eeg_channels[ch]] = volts;
                    }
                    else
                    {
                        safe_logger (spdlog::level::warn,
                            "EEG channel index {} out of bounds for package", eeg_channels[ch]);
                    }
                }

                // Add synced timestamp
                if (timestamp_channel >= 0 && timestamp_channel < num_rows)
                {
                    package[timestamp_channel] = synced_timestamp;
                }
                else
                {
                    safe_logger (spdlog::level::warn,
                        "Timestamp channel index {} out of bounds for package", timestamp_channel);
                }

                // Add marker (default to 0)
                if (marker_channel >= 0 && marker_channel < num_rows)
                {
                    package[marker_channel] = 0.0;
                }
                else
                {
                    safe_logger (spdlog::level::warn,
                        "Marker channel index {} out of bounds for package", marker_channel);
                }

                // Push the package to the buffer
                if (!package.empty () && (int)package.size () == num_rows)
                {
                    push_package (package.data ());

                    // Measure time in between packets
                    // TODO Delete?
                    static auto last_packet_time = std::chrono::steady_clock::now ();
                    static int packet_count = 0;
                    packet_count++;
                    auto now = std::chrono::steady_clock::now ();
                    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds> (
                        now - last_packet_time);
                    if (sample_counter % LOG_FREQUENCY == 0)
                    {
                        safe_logger (spdlog::level::info, "Packet #{}: {}ms since last packet",
                            packet_count, time_diff.count ());
                    }
                    last_packet_time = now;
                }
                else
                {
                    safe_logger (spdlog::level::err, "Package size mismatch or empty package");
                }

                // Log every 500 samples: all channels and timestamp
                sample_counter++;
                if (sample_counter % LOG_FREQUENCY == 0)
                {
                    // Build a string with all channel values and timestamp
                    std::string log_msg = "Sample #" + std::to_string (sample_counter) + " | ";
                    for (int i = 0; i < num_rows; ++i)
                    {
                        log_msg += "ch" + std::to_string (i) + "=" + std::to_string (package[i]);
                        if (i != num_rows - 1)
                        {
                            log_msg += ", ";
                        }
                    }
                    safe_logger (spdlog::level::info, log_msg);
                }

                // Set state and notify if first package
                if (this->state != (int)BrainFlowExitCodes::STATUS_OK)
                {
                    safe_logger (
                        spdlog::level::info, "received first package streaming is started");
                    {
                        std::lock_guard<std::mutex> lk (this->m);
                        this->state = (int)BrainFlowExitCodes::STATUS_OK;
                    }
                    this->cv.notify_one ();
                    safe_logger (spdlog::level::info, "start streaming");
                }

                buffer_pos += PACKET_TOTAL_SIZE;
            }
            else
            {
                // Not a start marker, move to next byte
                buffer_pos += 1;
            }
        }
        // If keep_alive is false, break out
        if (!keep_alive)
        {
            break;
        }
    }
}

/* This will send a message to stop sending data
It creates a spare buffer to read any spare bytes coming in from the board.
If the board keep sending more and more, up to the limit of 40000 bytes, send an error message */
/* int Cerelog_X8::stop_stream() {
    if (is_streaming) {
        keep_alive = false;
        is_streaming = false;
        streaming_thread.join(); //no idea what this does
        this->state = (int)BrainFlowExitCodes::SYNC_TIMEOUT_ERROR;
        // Now send the STOP_STREAMING command to the board and clear the serial buffer
        unsigned char buffer;
        int res = 1;
        int max_attempts = 400000;
        int cur_attempts = 0;
        while (res == 1) {
            res = serial->read_from_serial_port(&buffer, 1);
            cur_attempts++;

            if (cur_attempts == max_attempts) {
                safe_logger(spdlog::level::err, "We told it to stop but it no listen bruv.");
                return (int)BrainFlowExitCodes::BOARD_WRITE_ERROR;
            }
        }
    }

    return 3;
} */

int Cerelog_X8::stop_stream ()
{
    // turn off flow
    safe_logger (spdlog::level::info, "STOP STREAM FUNCTION CALLED BABY");
    if (is_streaming)
    {
        keep_alive = false;
        is_streaming = false;
        if (streaming_thread.joinable ())
        {
            streaming_thread.join ();
        }
        return (int)BrainFlowExitCodes::STATUS_OK;
    }
    else
    {
        return (int)BrainFlowExitCodes::STREAM_THREAD_IS_NOT_RUNNING;
    }
}

/* This function calls the stop_stream() function */
int Cerelog_X8::release_session ()
{
    if (initialized)
    {
        if (is_streaming)
        {
            this->stop_stream (); // Use Cerelog_X8's stop_stream() method
        }

        free_packages ();
        initialized = false;
        if (serial)
        {
            delete serial;
            serial = NULL;
        }
    }

    return (int)BrainFlowExitCodes::STATUS_OK;
}

double Cerelog_X8::convert_counter_to_timestamp (uint64_t packet_counter)
{
    if (!Cerelog_X8::sync_established)
    {
        first_packet_counter = packet_counter;
        first_packet_timestamp = time (nullptr); // is this helper function activated
        Cerelog_X8::sync_established = true;
        Cerelog_X8::last_sync_counter = packet_counter;
        Cerelog_X8::last_sync_timestamp = first_packet_timestamp;
        return first_packet_timestamp;
    }

    return 3;
}

// Computes a simple checksum by summing all bytes in the buffer and returning the result as uint8_t
uint8_t Cerelog_X8::calculate_checksum (const uint8_t *data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++)
    {
        checksum += data[i];
    }
    return checksum;
}

// Port scanning implementation
std::string Cerelog_X8::scan_for_device_port ()
{
    std::string os = get_port_info ().os;
    std::vector<std::string> ports_to_try;

    if (os == "Windows")
    {
        // Try common Windows USB serial patterns (COM 1-20)
        for (int i = 1; i <= 20; i++)
        {
            ports_to_try.push_back ("COM" + std::to_string (i));
        }
    }
    else if (os == "Darwin")
    {
        // Try common macOS USB serial patterns
        ports_to_try = {"/dev/cu.usbserial-110", "/dev/cu.usbserial-111", "/dev/cu.usbserial-112",
            "/dev/cu.usbserial-10", "/dev/cu.usbserial-11", "/dev/cu.usbserial-12",
            "/dev/cu.usbserial-210", "/dev/cu.usbserial-211", "/dev/cu.usbserial-212",
            "/dev/tty.usbserial-110", "/dev/tty.usbserial-111", "/dev/tty.usbserial-112",
            "/dev/tty.usbserial-210", "/dev/tty.usbserial-211", "/dev/tty.usbserial-212"};
    }
    else if (os == "Linux")
    {
        // Try common Linux USB serial patterns
        ports_to_try = {"/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyACM0",
            "/dev/ttyACM1", "/dev/ttyACM2"};
    }

    // Try to open each port
    for (const auto &port : ports_to_try)
    {
        OSSerial test_serial (port.c_str ());
        int result = test_serial.open_serial_port ();
        if (result >= 0)
        {
            test_serial.close_serial_port ();
            safe_logger (spdlog::level::info, "Found available port: {}", port);
            return port;
        }
    }

    // If no ports found, return default
    safe_logger (spdlog::level::warn, "No available ports found, using default");

    // Return OS-specific default port as fallback
    if (os == "Windows")
    {
        return "COM4";
    }
    else if (os == "Darwin")
    {
        return "/dev/cu.usbserial-110";
    }
    else if (os == "Linux")
    {
        return "/dev/ttyUSB0";
    }
    else
    {
        return "/dev/ttyUSB0"; // Generic fallback
    }
}

/* Function to convert config value to baud rate */
int Cerelog_X8::get_baud_rate_from_config (uint8_t config_val)
{
    switch (config_val) {
        case 0x00: return 9600; // default
        case 0x01: return 19200;
        case 0x02: return 38400; 
        case 0x03: return 57600;
        case 0x04: return 115200; 
        case 0x05: return 230400; // MacOS limit
        case 0x06: return 460800;
        case 0x07: return 921600; // Windows limit
        default: return -1; // Invalid config
        // TODO: add error message and Linux / fallback limits
    }
}
