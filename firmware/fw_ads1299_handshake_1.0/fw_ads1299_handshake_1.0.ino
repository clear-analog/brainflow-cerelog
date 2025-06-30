#include <Arduino.h>
#include <SPI.h>

// #define DEBUG_ENABLED // Uncomment when debugging ONLY - will corrupt data otherwise

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__) // accepts variable arguments
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

// --- Packet/Protocol Global Variables ---
// Number of status bytes from ADS1299 (fixed by chip)
const uint8_t ADS1299_NUM_STATUS_BYTES = 3;
// Number of channels (fixed by chip)
const uint8_t ADS1299_NUM_CHANNELS = 8;
// Number of bytes per channel (fixed by chip)
const uint8_t ADS1299_BYTES_PER_CHANNEL = 3;
// Total number of data bytes from ADS1299 (status + channel data)
const uint8_t ADS1299_TOTAL_DATA_BYTES = ADS1299_NUM_STATUS_BYTES + (ADS1299_NUM_CHANNELS * ADS1299_BYTES_PER_CHANNEL);

// Packet protocol fields
const uint8_t PACKET_TIMESTAMP_BYTES = 4; 
const uint8_t PACKET_START_MARKER_BYTES = 2;
const uint8_t PACKET_END_MARKER_BYTES = 2;
const uint8_t PACKET_LENGTH_FIELD_BYTES = 1;
const uint8_t PACKET_CHECKSUM_BYTES = 1;

// The message length field (timestamp + ADS1299 data)
const uint8_t PACKET_MSG_LENGTH = PACKET_TIMESTAMP_BYTES + ADS1299_TOTAL_DATA_BYTES;

// The total packet size (start + len + data + checksum + end)
const uint8_t PACKET_TOTAL_SIZE = PACKET_START_MARKER_BYTES + PACKET_LENGTH_FIELD_BYTES + PACKET_MSG_LENGTH + PACKET_CHECKSUM_BYTES + PACKET_END_MARKER_BYTES;

// Indices for packet fields
const uint8_t PACKET_IDX_START_MARKER = 0;
const uint8_t PACKET_IDX_LENGTH = PACKET_IDX_START_MARKER + PACKET_START_MARKER_BYTES;
const uint8_t PACKET_IDX_TIMESTAMP = PACKET_IDX_LENGTH + PACKET_LENGTH_FIELD_BYTES;
const uint8_t PACKET_IDX_ADS1299_DATA = PACKET_IDX_TIMESTAMP + PACKET_TIMESTAMP_BYTES;
const uint8_t PACKET_IDX_CHECKSUM = PACKET_IDX_ADS1299_DATA + ADS1299_TOTAL_DATA_BYTES;
const uint8_t PACKET_IDX_END_MARKER = PACKET_IDX_CHECKSUM + PACKET_CHECKSUM_BYTES;

// --- Pin Mapping ---
static const uint8_t pin_MOSI_NUM = 23;
static const uint8_t pin_CS_NUM = 5;
static const uint8_t pin_MISO_NUM = 19;
static const uint8_t pin_SCK_NUM = 18;
static const uint8_t pin_PWDN_NUM = 13;
static const uint8_t pin_RST_NUM = 12;
static const uint8_t pin_START_NUM = 14;
static const uint8_t pin_DRDY_NUM = 27;
static const uint8_t pin_LED_DEBUG = 17;

// --- SPI instance ---
SPIClass *vspi = NULL; // SPI instance

// Query Timing Setup
unsigned long _last_query_time = 0;
static const int SPI_FREQ = 4000000;
static const int SAMPLE_FREQ = 500;
static const float SAMPLE_PRD_us = (1 / SAMPLE_FREQ) * 1000000; // This is to be used in delayMicroseconds

// --- ADS1299 State Management ---
int _ADS1299_MODE = -2;
int ADS1299_MODE_SDATAC = 1;
int ADS1299_MODE_RDATAC = 2;

int _ADS1299_PREV_CMD = -1;
int _CMD_ADC_WREG = 3;
int _CMD_ADC_RREG = 4;
int _CMD_ADC_SDATAC = 17;
int _CMD_ADC_RDATAC = 16;
int _CMD_ADC_START = 8;

// --- Interrupt Flag ---
volatile bool dataReady = false;

// --- Unix Timestamp Tracking ---
unsigned long _unix_timestamp_reference = 0; // Unix timestamp when we started
unsigned long _millis_reference = 0;         // millis() value when we started
bool _timestamp_initialized = false;

// --- Function Prototypes ---
void ADS1299_WREG(uint8_t regAdd, uint8_t *values, uint8_t numRegs);
void ADS1299_RREG(uint8_t regAdd, uint8_t *buffer, uint8_t numRegs);
void ADS1299_SETUP(void);
void ADS1299_SDATAC(void);
void ADS1299_RDATAC(void);
void ADS1299_START(void);
byte SPI_SendByte(byte data_byte, bool cont);
void read_ADS1299_data(byte *buffer);
void IRAM_ATTR onDRDYFalling(void);
void print_all_ADS1299_registers_from_setup(void);
uint32_t get_baud_rate_from_config(uint8_t config_val);

// --- Register Setup ---
/* Data Structure for controlling registers */
typedef struct Deez {
    int add;
    int reg_val;
} regVal_pair;
const int size_reg_ls = 24;
// Registers to setup. If -2, end WREG.
static const regVal_pair ADS1299_REGISTER_LS[size_reg_ls] = {
    {0x01, 0b10110101},
    {0x02, 0b11010000},
    {0x03, 0b11101100},
    {0x04, 0},
    {-2, -2},
    {0x05, 0b01100000},
    {0x06, 0b01100000},
    {0x07, 0b01100000},
    {0x08, 0b01100000},
    {0x09, 0b01100000},
    {0x0A, 0b01100000},
    {0x0B, 0b01100000},
    {0x0C, 0b01100000},
    {0x0D, 0b11111111},
    {0x0E, 0b11111111},
    {0x0F, 0},
    {0x10, 0},
    {0x11, 0},
    {-2, -2},
    {0x15, 0},
    {0x16, 0},
    {0x17, 0}};

// --- Timestamp from BrainFlow --- 
int handshake_packet_size = 12;
/* Looking for 12 byte handshake packet 
    [0xAA][0xBB]  // start marker
    [message_type]
    [timestamp][timestamp][timestamp][timestamp]
    [register_address][register_value]
    [checksum]
    [0xCC][0xDD]  // end marker
*/ 
const uint8_t MSG_TYPE_TIMESTAMP = 0x02;
const uint8_t HANDSHAKE_START_MARKER_1 = 0xAA;
const uint8_t HANDSHAKE_START_MARKER_2 = 0xBB;
const uint8_t HANDSHAKE_END_MARKER_1 = 0xCC;
const uint8_t HANDSHAKE_END_MARKER_2 = 0xDD;

// Ring buffer for handshake detection
const uint8_t RING_BUFFER_SIZE = 24; // 2x handshake packet size so message doesn't get cut off
uint8_t ring_buffer[RING_BUFFER_SIZE];
uint8_t ring_head = 0;
uint8_t ring_tail = 0;
uint8_t ring_counter = 0;

bool waitForTimestamp() {
    // Read available data into ring buffer
    while (Serial.available() > 0 && ring_counter < RING_BUFFER_SIZE) {
        ring_buffer[ring_head] = Serial.read();
        ring_head = (ring_head + 1) % RING_BUFFER_SIZE;
        ring_counter++;
    }
    if (ring_counter < handshake_packet_size) {
        return false; // Not enough data yet
    }

    // Scan the filled buffer for the start marker
    for (uint8_t i = 0; i < handshake_packet_size + 3; i++) { // don't technically need end marker or checksum to read message
        uint8_t ring_index = (ring_tail + i) % RING_BUFFER_SIZE;
        if (ring_buffer[ring_index] == HANDSHAKE_START_MARKER_1 && 
            ring_buffer[(ring_index + 1) % RING_BUFFER_SIZE] == HANDSHAKE_START_MARKER_2 && 
            ring_buffer[(ring_index + 2) % RING_BUFFER_SIZE] == MSG_TYPE_TIMESTAMP) {
            
            // Read timestamp (4 bytes)
            uint32_t received_timestamp = 0;
            received_timestamp |= (uint32_t)ring_buffer[(ring_index + 3) % RING_BUFFER_SIZE] << 24;
            received_timestamp |= (uint32_t)ring_buffer[(ring_index + 4) % RING_BUFFER_SIZE] << 16;
            received_timestamp |= (uint32_t)ring_buffer[(ring_index + 5) % RING_BUFFER_SIZE] << 8;
            received_timestamp |= (uint32_t)ring_buffer[(ring_index + 6) % RING_BUFFER_SIZE];

            // Read parameters (2 bytes)
            uint8_t reg_addr = ring_buffer[(ring_index + 7) % RING_BUFFER_SIZE];
            uint8_t reg_val = ring_buffer[(ring_index + 8) % RING_BUFFER_SIZE];

            // Baud rate configuration
            if (reg_addr == 0x01) { // baud rate register
                uint32_t new_baud_rate = get_baud_rate_from_config(reg_val);
                if (new_baud_rate > 0) {
                    DEBUG_PRINT("Current baud rate: ");
                    DEBUG_PRINTLN(Serial.baudRate());
                    DEBUG_PRINT("Switching to baud rate: ");
                    DEBUG_PRINTLN(new_baud_rate);
                    Serial.flush(); // Wait for all data to be transmitted
                    delay(100);
                    Serial.begin(new_baud_rate);
                    delay(100);
                    DEBUG_PRINT("Baud rate successfully switched to: ");
                    DEBUG_PRINTLN(Serial.baudRate());
                } else {
                    DEBUG_PRINTLN("Invalid baud rate configuration");
                }
            } else if (reg_addr != 0x00) {
                // Apply other register settings
                DEBUG_PRINT("User parameter received: ");
                DEBUG_PRINTLN(reg_addr);
            }

            // Set timestamp and switch baud rate
            _unix_timestamp_reference = received_timestamp;
            _millis_reference = millis();
            _timestamp_initialized = true;
            delay(100);
            DEBUG_PRINT("Received timestamp: ");
            DEBUG_PRINTLN(received_timestamp);
            return true;
        }
    }
    return false; // data is not a handshake packet
}

// --- Interrupt Service Routine ---
void IRAM_ATTR onDRDYFalling(void) {
    dataReady = true;
}

// --- SPI Send/Receive Byte (Arduino Framework) ---
/* Cont represents continuos. So if we are sending bytes without*/
byte SPI_SendByte(byte data_byte, bool cont){
    if (!cont){
        digitalWrite(pin_CS_NUM, LOW); // Assert CS
    }

    byte received = vspi->transfer(data_byte); // Send and receive

    if (!cont) {
        digitalWrite(pin_CS_NUM, HIGH); // De-assert CS
    }
    return received;
}

// --- ADS1299 Write Registers (Arduino) ---
void ADS1299_WREG(uint8_t regAdd, uint8_t *values, uint8_t numRegs){
    if (_ADS1299_MODE != ADS1299_MODE_SDATAC){
        ADS1299_SDATAC();
    }
    digitalWrite(pin_CS_NUM, LOW);
    // SPI_SendByte(0x40 | (regAdd & 0x1F), true);
    SPI_SendByte(0b01000000 | regAdd, true);
    // delayMicroseconds(2);
    SPI_SendByte(numRegs - 1, true);
    DEBUG_PRINT("Reg Add: ");
    DEBUG_PRINTLN(0b0000000100000000 + regAdd, BIN);
    DEBUG_PRINT("Actual byte sent over to indicate register address: ");
    DEBUG_PRINTLN(0b0000000100000000 + (0b01000000 | regAdd), BIN);

    for (uint8_t i = 0; i < numRegs; i++) {
        SPI_SendByte(values[i], true);
        DEBUG_PRINT("Register value sent: ");
        DEBUG_PRINTLN(0b0000000100000000 + values[i], BIN);
    }
    digitalWrite(pin_CS_NUM, HIGH);
    _ADS1299_PREV_CMD = _CMD_ADC_WREG;

    // is this separate from handshake? 
    // take in parameters to change e.g. sampling rate, gain, serial port, enable
    // configuration params from Brainflow
    // BF params to register map
}

// --- ADS1299 Read Registers (Arduino) ---
// May need to implement timer in between the two bytes being sent
void ADS1299_RREG(uint8_t regAdd, uint8_t *buffer, uint8_t numRegs) {
    if (_ADS1299_MODE != ADS1299_MODE_SDATAC) {
        ADS1299_SDATAC();
    }
    digitalWrite(pin_CS_NUM, LOW);
    // SPI_SendByte(0x20 | (regAdd & 0x1F), true);
    SPI_SendByte(0b00100000 | regAdd, true);
    // delayMicroseconds(2);
    SPI_SendByte(numRegs - 1, true);
    DEBUG_PRINT("Reg Add: ");
    DEBUG_PRINTLN(0b0000000100000000 + regAdd, BIN);
    DEBUG_PRINT("Actual byte sent over to indicate register address: ");
    DEBUG_PRINTLN(0b0000000100000000 + (0b01000000 | regAdd), BIN);
    // delayMicroseconds(2);
    for (uint8_t i = 0; i < numRegs; i++) {
        buffer[i] = SPI_SendByte(0x00, true); // Clock in data with dummy bytes
    }
    digitalWrite(pin_CS_NUM, HIGH);
    _ADS1299_PREV_CMD = _CMD_ADC_RREG;
}

// --- ADS1299 SDATAC ---
void ADS1299_SDATAC(void) {
    SPI_SendByte(_CMD_ADC_SDATAC, false);
    _ADS1299_MODE = ADS1299_MODE_SDATAC;
    _ADS1299_PREV_CMD = _CMD_ADC_SDATAC;
    DEBUG_PRINTLN("Sent SDATAC command!");
}

// --- ADS1299 RDATAC ---
void ADS1299_RDATAC(void) {
    SPI_SendByte(_CMD_ADC_RDATAC, false);
    _ADS1299_MODE = ADS1299_MODE_RDATAC;
    _ADS1299_PREV_CMD = _CMD_ADC_RDATAC;
    DEBUG_PRINTLN("Send RDATAC command!");
}

// --- ADS1299 Start Command --
void ADS1299_START(void) {
    SPI_SendByte(_CMD_ADC_START, false);
    _ADS1299_PREV_CMD = _CMD_ADC_START;
    DEBUG_PRINTLN("Sent START command!");
}

// --- ADS1299 Setup (Arduino) ---
void ADS1299_SETUP(void) {
    digitalWrite(pin_PWDN_NUM, LOW);
    digitalWrite(pin_RST_NUM, LOW);
    DEBUG_PRINTLN("Init pins low");
    delay(100);

    digitalWrite(pin_PWDN_NUM, HIGH);
    digitalWrite(pin_RST_NUM, HIGH);
    DEBUG_PRINTLN("Init pins high");
    delay(100);

    ADS1299_SDATAC();

    uint8_t refbuf[] = {0b11101100}; // 0b11100000 IS OLD SETTING
    ADS1299_WREG(0x03, refbuf, 1);
    delay(10);

    uint8_t value[1]; // register setting placeholder var
    uint8_t i = 0;
    while (i < size_reg_ls) {
        const regVal_pair temp = ADS1299_REGISTER_LS[i];
        if (temp.add == -2) { // stop transaction and start new one
            i++;
            continue;
        }
        value[0] = {(uint8_t)temp.reg_val};
        ADS1299_WREG(temp.add, value, 1);
        delayMicroseconds(2); // Short delay between register writes
        i++;
    }
}

/* Read ADS1299 Data and Accepts Byte Array */
void read_ADS1299_data(byte *buffer) {
    digitalWrite(pin_CS_NUM, LOW);
    DEBUG_PRINT("Next packet:");
    for (int i = 0; i < ADS1299_TOTAL_DATA_BYTES; i++) { // 3 status bytes + 8 channels * 3 bytes/channel
        buffer[i] = SPI_SendByte(0x00, true);
        DEBUG_PRINT(' %d\n', buffer[i]);
    }
    DEBUG_PRINTLN();
    digitalWrite(pin_CS_NUM, HIGH);
}

// --- Print all relevant ADS1299 registers after setup, using addresses from ADS1299_REGISTER_LS ---
void print_all_ADS1299_registers_from_setup(void) {
    DEBUG_PRINTLN("---- ADS1299 Register Dump ----");
    for (int i = 0; i < size_reg_ls; i++) {
        int reg_addr = ADS1299_REGISTER_LS[i].add;
        if (reg_addr == -2) {
            i++;
            continue;
        } // skip marker
        uint8_t reg_val[1];
        ADS1299_RREG((uint8_t)reg_addr, reg_val, 1);
        DEBUG_PRINT("Register 0x");
        if (reg_addr < 0x10) {
            DEBUG_PRINT("0");
        }

        DEBUG_PRINT(reg_addr, HEX);
        DEBUG_PRINT(" : ");
        // Add 0b0000000100000000 (0x100) to force leading 1 for 8 bits
        uint16_t val_for_print = 0x100 | reg_val[0];
        DEBUG_PRINTLN(val_for_print, BIN); // User can ignore the first '1'
        delayMicroseconds(2);                               // Small delay between reads
    }
    DEBUG_PRINTLN("-------------------------------");
}

uint32_t get_baud_rate_from_config(uint8_t config_val) {
    switch (config_val) {
        case 0x00: return 9600;    // default
        case 0x01: return 19200;
        case 0x02: return 38400; 
        case 0x03: return 57600;
        case 0x04: return 115200;  // fallback?
        case 0x05: return 230400;  // MacOS limit
        case 0x06: return 460800;
        case 0x07: return 921600;  // Windows limit
        default: return 0;         // Invalid config
    }
}

// SETUP FUNCTION
void setup() {
    // --- Serial Initialization ---
    Serial.begin(9600);
    
    #ifdef DEBUG_ENABLED
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("!!! WARNING: DEBUG MODE ENABLED - INCOMING DATA WILL BE CORRUPTED AND TESTS WILL FAIL !!!");
        Serial.println("!!! REMEMBER TO DISABLE DEBUG MODE BEFORE UPLOADING TO PRODUCTION !!!");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("Message will self-destruct in 5 seconds...");
        delay(5000);
    #endif
    
    /* GPIO Configuration */
    pinMode(pin_PWDN_NUM, OUTPUT);
    pinMode(pin_RST_NUM, OUTPUT);
    pinMode(pin_START_NUM, OUTPUT);
    pinMode(pin_CS_NUM, OUTPUT);
    pinMode(pin_DRDY_NUM, INPUT_PULLUP); // Use internal pull-up
    pinMode(pin_LED_DEBUG, OUTPUT);
    digitalWrite(pin_CS_NUM, HIGH); // Initialize CS high
    delay(2000);

    digitalWrite(pin_LED_DEBUG, LOW);

    // --- SPI Initialization ---
    // Check if we're using ESP32 or AVR
    #if defined(ESP32)
        // ESP32 SPI initialization
        vspi = new SPIClass(VSPI);                                          // Create VSPI instance
        vspi->begin(pin_SCK_NUM, pin_MISO_NUM, pin_MOSI_NUM, pin_CS_NUM);   // Initialize SPI
        vspi->beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1)); // 4 MHz SPI clock, MSB first, Mode 1 because CPOL = 0 and CPHA = 1
    #else
        // Standard Arduino SPI initialization
        // vspi = &SPI;  // Use the default SPI instance
        vspi->begin(pin_SCK_NUM, pin_MISO_NUM, pin_MOSI_NUM, pin_CS_NUM);
        vspi->beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1)); // 8 MHz SPI clock, MSB first, Mode 1 because CPOL = 0 and CPHA = 1
    #endif
    delay(500);

    // --- ADS1299 Initialization ---
    ADS1299_SETUP();

    // --- Timestamp fallback ---
    DEBUG_PRINTLN("Waiting for timestamp from BrainFlow...");
    if (!waitForTimestamp()) { 
        DEBUG_PRINTLN("No timestamp received - using fallback");
        _unix_timestamp_reference = 1500000000UL;
        _millis_reference = millis();
        _timestamp_initialized = true;
    } else {
        DEBUG_PRINTLN("Timestamp handshake successful!");
    }

    // --- Print all relevant registers before starting continuous data read ---
    print_all_ADS1299_registers_from_setup();

    // --- Attach Interrupt ---
    attachInterrupt(digitalPinToInterrupt(pin_DRDY_NUM), onDRDYFalling, FALLING);

    // --- Fallback timestamp after timeout ---
    if (!_timestamp_initialized) {
        DEBUG_PRINTLN("Timeout waiting for timestamp. Using fallback timestamp.");
        _unix_timestamp_reference = 1500000000UL;  // 1.5B is July 2017
        _millis_reference = millis();
        _timestamp_initialized = true;
    }

    DEBUG_PRINTLN("Setup complete.");
    digitalWrite(pin_START_NUM, HIGH);
    ADS1299_START();
    delay(1); // wait for 1 ms
    ADS1299_RDATAC();
    delay(100);
    ADS1299_START();

    // Turn on ESP32 light
    #ifdef DEBUG_ENABLED
        static int led_counter = 0;
        digitalWrite(pin_LED_DEBUG, (++led_counter % SAMPLE_FREQ) < (SAMPLE_FREQ * 3 / 4)); // blink light in DEBUG mode
    #else
        digitalWrite(pin_LED_DEBUG, HIGH);
    #endif
}

// LOOP FUNCTION
void loop() {
    // Check for handshake in between data transmissions
    if (Serial.available() >= 12) {
        if (waitForTimestamp()) {
            return; // skip cycle to avoid timing issues
        }        
    }
    
    unsigned long currentMicros = micros();
    if (currentMicros - _last_query_time >= SAMPLE_PRD_us) {
        _last_query_time = currentMicros;
        if (dataReady) {
            dataReady = false;
          
            // Read ADS1299 data
            byte raw_data[ADS1299_TOTAL_DATA_BYTES];
            read_ADS1299_data(raw_data);

            // Create message buffer
            const uint16_t START_MARKER = 0xABCD; // 2 bytes
            const uint16_t END_MARKER = 0xDCBA;   // 2 bytes
            byte packet[PACKET_TOTAL_SIZE];       // start + len + data + checksum + end

            // Start marker (2 bytes)
            packet[PACKET_IDX_START_MARKER] = (START_MARKER >> 8) & 0xFF;
            packet[PACKET_IDX_START_MARKER + 1] = START_MARKER & 0xFF;

            // Message length (1 byte)
            packet[PACKET_IDX_LENGTH] = PACKET_MSG_LENGTH;

            // UNIX timestamp (4 bytes)
            uint32_t timestamp = _unix_timestamp_reference + ((millis() - _millis_reference) / 1000); // old: uint32_t timestamp = (uint32_t)(packet_counter & 0xFFFFFFFF);  // Only send lower 4 bytes
            #ifdef DEBUG_ENABLED 
                static int debug_counter = 0;
                if (++debug_counter % 100 == 0) { // print every 100 packets
                    Serial.print("DEBUG: ref=");
                    Serial.print(_unix_timestamp_reference);
                    Serial.print(", elapsed=");
                    Serial.print((millis() - _millis_reference) / 1000);
                    Serial.print(", timestamp=");
                    Serial.println(timestamp);
                } 
            #endif
            packet[PACKET_IDX_TIMESTAMP] = (timestamp >> 24) & 0xFF;
            packet[PACKET_IDX_TIMESTAMP + 1] = (timestamp >> 16) & 0xFF;
            packet[PACKET_IDX_TIMESTAMP + 2] = (timestamp >> 8) & 0xFF;
            packet[PACKET_IDX_TIMESTAMP + 3] = timestamp & 0xFF;

            // Copy ADS1299 data (27 bytes)
            for (uint8_t i = 0; i < ADS1299_TOTAL_DATA_BYTES; i++) {
                packet[PACKET_IDX_ADS1299_DATA + i] = raw_data[i];
            }

            // Compute checksum (sum of all bytes from length to last data byte)
            uint8_t checksum = 0;
            for (uint8_t i = PACKET_IDX_LENGTH; i < PACKET_IDX_CHECKSUM; i++) {
                checksum += packet[i];
            }
            packet[PACKET_IDX_CHECKSUM] = checksum;

            // End marker (2 bytes)
            packet[PACKET_IDX_END_MARKER] = (END_MARKER >> 8) & 0xFF;
            packet[PACKET_IDX_END_MARKER + 1] = END_MARKER & 0xFF;

            // Send entire packet over Serial
            Serial.write(packet, sizeof(packet));
        }
    }
}