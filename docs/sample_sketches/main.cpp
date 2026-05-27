#include <Arduino.h>
#include "HoverboardAPI.h"

// --- Configuration ---
#define HOVER_SERIAL Serial2
#define HOVER_RX_PIN 16
#define HOVER_TX_PIN 17
#define HOVER_BAUD   115200

uint32_t bytesReceived = 0;

/**
 * Sliding window parser to catch the Hoverboard's Settings acknowledgment.
 * Reads byte-by-byte, shifts the buffer, and validates the Magic Number and Checksum.
 */
bool waitForSettingsResponse(HoverboardSettingsCommand *response, uint32_t timeout_ms) {
    uint32_t startMillis = millis();
    uint8_t buffer[sizeof(HoverboardSettingsCommand)];
    int bufIdx = 0;
    bytesReceived = 0;

    while (millis() - startMillis < timeout_ms) {
        while (HOVER_SERIAL.available()) {
            uint8_t b = HOVER_SERIAL.read();
            bytesReceived++;
            buffer[bufIdx++] = b;

            // Once we have enough bytes for a full frame
            if (bufIdx == sizeof(HoverboardSettingsCommand)) {
                HoverboardSettingsCommand *cmd = (HoverboardSettingsCommand *)buffer;

                // 1. Check Magic Number
                if (cmd->packet_start == SERIAL_SETTINGS_FRAME) {
                    
                    // 2. Check Checksum
                    uint16_t calculated_sum = 0;
                    size_t payload_size = sizeof(HoverboardSettingsCommand) - sizeof(cmd->checksum);
                    for(size_t i = 0; i < payload_size; i++) {
                        calculated_sum += buffer[i];
                    }

                    if (calculated_sum == cmd->checksum) {
                        memcpy(response, buffer, sizeof(HoverboardSettingsCommand));
                        return true; // Packet is valid!
                    } else {
                        Serial.printf("   [Debug] Checksum mismatch. Expected: %d, Got: %d\n", cmd->checksum, calculated_sum);
                    }
                }
                
                // Invalid packet or checksum failed: shift the sliding window left by 1 byte
                memmove(buffer, buffer + 1, sizeof(HoverboardSettingsCommand) - 1);
                bufIdx--;
            }
        }
    }
    return false; // Timeout
}

/**
 * Sliding window parser for the continuous 72-byte Telemetry stream.
 */
bool receiveTelemetry(HoverboardFeedback *feedback) {
    static uint8_t buffer[sizeof(HoverboardFeedback)];
    static int bufIdx = 0;

    while (HOVER_SERIAL.available()) {
        buffer[bufIdx++] = HOVER_SERIAL.read();

        // Once we have enough bytes for a full frame
        if (bufIdx == sizeof(HoverboardFeedback)) {
            HoverboardFeedback *fb = (HoverboardFeedback *)buffer;

            if (fb->start == SERIAL_START_FRAME) {
                uint16_t calculated_sum = 0;
                for (size_t i = 0; i < sizeof(HoverboardFeedback) - 2; i++) {
                    calculated_sum += buffer[i];
                }

                if (calculated_sum == fb->checksum) {
                    memcpy(feedback, buffer, sizeof(HoverboardFeedback));
                    bufIdx = 0; 
                    return true;
                }
            }
            
            memmove(buffer, buffer + 1, sizeof(HoverboardFeedback) - 1);
            bufIdx--;
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    HOVER_SERIAL.begin(HOVER_BAUD, SERIAL_8N1, HOVER_RX_PIN, HOVER_TX_PIN);
    
    delay(3000); // Give the user time to open the serial monitor
    Serial.println("\n\n=== Hoverboard Binary Protocol Test Rig ===");
    
    HoverboardSettingsCommand cmdOut, cmdIn;

    // ---------------------------------------------------------
    // TEST 1: Change Maximum Motor Speed (PARAM_N_MOT_MAX) to 1001
    // ---------------------------------------------------------
    // Clear the RX buffer from stagnant telemetry before sending a command
    while(HOVER_SERIAL.available()) HOVER_SERIAL.read();

    Serial.print("TEST 1: Setting N_MOT_MAX to 1001 RPM... ");
    Hoverboard_BuildSettings(&cmdOut, CMD_SET, PARAM_N_MOT_MAX, 1001);
    HOVER_SERIAL.write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 1000)) {
        if (cmdIn.error == ERR_OK && cmdIn.param_value == 1001) {
            Serial.println("PASS!");
            Serial.printf("   -> Target: %d | Hoverboard confirmed value: %d\n", 1001, cmdIn.param_value);
        } else {
            Serial.printf("FAIL! (Hoverboard threw error code: %d. Returned value: %d)\n", cmdIn.error, cmdIn.param_value);
            if (cmdIn.error == ERR_VALUE_OUT_OF_BOUNDS) Serial.println("   -> Reason: Value out of bounds.");
        }
    } else {
        Serial.println("FAIL! (Timeout waiting for Hoverboard acknowledgment)");
    }
    Serial.printf("   [Debug] Bytes received during window: %d\n\n", bytesReceived);
    delay(1500);


    // ---------------------------------------------------------
    // TEST 2: Reset Maximum Motor Speed to Defaults (INIT)
    // ---------------------------------------------------------
    // Clear the RX buffer from stagnant telemetry before sending a command
    while(HOVER_SERIAL.available()) HOVER_SERIAL.read();

    Serial.print("TEST 2: Initializing N_MOT_MAX to default firmware value... ");
    Hoverboard_BuildSettings(&cmdOut, CMD_INIT, PARAM_N_MOT_MAX, 0);
    HOVER_SERIAL.write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 1000)) {
        if (cmdIn.error == ERR_OK) {
            Serial.println("PASS!");
            Serial.printf("   -> Hoverboard restored value to: %d RPM\n", cmdIn.param_value);
        } else {
            Serial.printf("FAIL! (Hoverboard threw error code: %d)\n", cmdIn.error);
        }
    } else {
        Serial.println("FAIL! (Timeout waiting for Hoverboard acknowledgment)");
    }
    Serial.printf("   [Debug] Bytes received during window: %d\n\n", bytesReceived);
    delay(1500);


    // ---------------------------------------------------------
    // TEST 3: -10% to +10% Speed Sweep 
    // ---------------------------------------------------------
    // Clear the RX buffer to ensure live telemetry starts perfectly synced
    while(HOVER_SERIAL.available()) HOVER_SERIAL.read();

    Serial.println("TEST 3: Sweeping motor speed from -100 to 100 with LIVE Telemetry...");
    HoverboardControlCommand ctrl;
    
    // Ramp Up
    for (int speed = -100; speed <= 100; speed += 2) {
        Hoverboard_BuildControl(&ctrl, 0, speed);
        HOVER_SERIAL.write((uint8_t*)&ctrl, sizeof(ctrl));
        
        // Spin-wait for ~20ms (50Hz loop) while actively parsing telemetry
        uint32_t t = millis();
        while(millis() - t < 20) {
            HoverboardFeedback fb;
            if (receiveTelemetry(&fb)) {
                Serial.printf("   [Live] Cmd: %4d | Meas SPD: %4d RPM | DC Link: %4.2fA | Bat: %4.2fV | Temp: %4.1fC\n", 
                              speed, fb.spd_avg, fb.dc_curr / 100.0, fb.batv / 100.0, fb.temp / 10.0);
            }
        }
    }
    
    // Ramp Down to 0
    for (int speed = 100; speed >= 0; speed -= 2) {
        Hoverboard_BuildControl(&ctrl, 0, speed);
        HOVER_SERIAL.write((uint8_t*)&ctrl, sizeof(ctrl));
        
        uint32_t t = millis();
        while(millis() - t < 20) {
            HoverboardFeedback fb;
            if (receiveTelemetry(&fb)) {
                Serial.printf("   [Live] Cmd: %4d | Meas SPD: %4d RPM | DC Link: %4.2fA | Bat: %4.2fV | Temp: %4.1fC\n", 
                              speed, fb.spd_avg, fb.dc_curr / 100.0, fb.batv / 100.0, fb.temp / 10.0);
            }
        }
    }

    Serial.println("Sweep complete. Motors should have ramped backward, forward, and stopped.\n");
    Serial.println("=== Test Suite Finished. Starting continuous telemetry view ===\n");
}

void loop() {
    HoverboardFeedback fb;
    static uint32_t lastPrint = 0;

    // Continuously parse incoming data to keep the UART buffer clear
    if (receiveTelemetry(&fb)) {
        // Print a readable dashboard every 500ms
        if (millis() - lastPrint > 500) {
            Serial.println("--- Hoverboard Stats ---");
            Serial.printf("Battery:   %.2f V\n", fb.batv / 100.0);
            Serial.printf("Temp:      %.1f C\n", fb.temp / 10.0);
            Serial.printf("Speed Avg: %d RPM\n", fb.spd_avg);
            Serial.printf("DC Curr:   %.2f A\n", fb.dc_curr / 100.0);
            Serial.printf("Rate Param:%d\n", fb.rate);
            Serial.println("------------------------\n");
            lastPrint = millis();
        }
    }
}