#pragma once

#include <thread>
#include <vector>

#include "board.h"
#include "board_controller.h"
#include "serial.h"

class Cerelog_X8 : public Board
{
private:
    volatile bool keep_alive;
    bool initialized;
    bool is_streaming;
    std::thread streaming_thread;
    Serial *serial;
    int state;
    std::mutex m;                      // This is for thread processing later on
    std::condition_variable cv;        // I don't really know what this is doing
    uint64_t first_packet_counter = 0; // data storers
    double first_packet_timestamp = 0.0;
    uint64_t last_sync_counter = 0;
    double last_sync_timestamp = 0.0;
    int sync_count = 0;
    bool sync_established = false;
    int sampling_rate = 500;
    int send_timestamp_handshake (uint8_t reg_addr = 0x00, uint8_t reg_val = 0x00);
    int get_baud_rate_from_config (uint8_t config_val);

    void read_thread ();
    double convert_counter_to_timestamp (uint64_t packet_counter);
    std::string scan_for_device_port ();

public:
    Cerelog_X8 (int board_id, struct BrainFlowInputParams params);
    int prepare_session ();
    int start_stream (int buffer_size, const char *streamer_params);
    int stop_stream ();
    int release_session ();
    int config_board (std::string config, std::string &response);
    uint8_t calculate_checksum (const uint8_t *data, size_t length);
};