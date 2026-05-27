#include "HoverboardAPI.h"
#include <string.h>

/**
 * Builds the high-frequency control packet.
 */
void Hoverboard_BuildControl(HoverboardControlCommand *cmd, int16_t steer, int16_t speed) {
    cmd->start = SERIAL_START_FRAME;
    cmd->steer = steer;
    cmd->speed = speed;
    cmd->checksum = (uint16_t)(cmd->start ^ cmd->steer ^ cmd->speed);
}

/**
 * Builds the settings configuration packet.
 */
void Hoverboard_BuildSettings(HoverboardSettingsCommand *cmd, HoverboardCommand_t action, HoverboardParam_t param, int32_t value) {
    cmd->packet_start  = SERIAL_SETTINGS_FRAME;
    cmd->semaphore     = 0;
    cmd->error         = 0;
    cmd->command_index = (int8_t)action;
    cmd->param_index   = (int8_t)param;
    cmd->param_value   = value;

    uint8_t *ptr = (uint8_t *)cmd;
    uint16_t calculated_sum = 0;
    size_t payload_size = sizeof(HoverboardSettingsCommand) - sizeof(cmd->checksum);
    for(size_t i = 0; i < payload_size; i++) {
        calculated_sum += ptr[i];
    }
    cmd->checksum = calculated_sum;
}

#ifdef __cplusplus

HoverSerial::HoverSerial(HardwareSerial& serial) {
    _serial = &serial;
    _telemetry_bufIdx = 0;
    memset(&_lastFeedback, 0, sizeof(HoverFeedback));
}

void HoverSerial::start(unsigned long baudrate, int8_t rx_pin, int8_t tx_pin) {
    if (rx_pin != -1 && tx_pin != -1) {
        _serial->begin(baudrate, SERIAL_8N1, rx_pin, tx_pin);
    } else {
        _serial->begin(baudrate);
    }
}

void HoverSerial::emptyBuffer() {
    while (_serial->available()) {
        _serial->read();
    }
}

bool HoverSerial::waitForSettingsResponse(HoverboardSettingsCommand *response, uint32_t timeout_ms) {
    uint32_t startMillis = millis();
    uint8_t buffer[sizeof(HoverboardSettingsCommand)];
    int bufIdx = 0;

    while (millis() - startMillis < timeout_ms) {
        while (_serial->available()) {
            buffer[bufIdx++] = _serial->read();

            if (bufIdx == sizeof(HoverboardSettingsCommand)) {
                HoverboardSettingsCommand *cmd = (HoverboardSettingsCommand *)buffer;
                if (cmd->packet_start == SERIAL_SETTINGS_FRAME) {
                    uint16_t calculated_sum = 0;
                    size_t payload_size = sizeof(HoverboardSettingsCommand) - sizeof(cmd->checksum);
                    for(size_t i = 0; i < payload_size; i++) { calculated_sum += buffer[i]; }
                    
                    if (calculated_sum == cmd->checksum) {
                        memcpy(response, buffer, sizeof(HoverboardSettingsCommand));
                        return true;
                    }
                }
                memmove(buffer, buffer + 1, sizeof(HoverboardSettingsCommand) - 1);
                bufIdx--;
            }
        }
    }
    return false; // Timeout
}

int8_t HoverSerial::setParam(HoverboardParam_t param, int32_t value) {
    emptyBuffer(); // Protect against hardware buffer overflows
    HoverboardSettingsCommand cmdOut, cmdIn;
    Hoverboard_BuildSettings(&cmdOut, CMD_SET, param, value);
    _serial->write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 500)) { // Hardcoded 500ms limit
        if (cmdIn.error == ERR_OK) return 0;    // Translated Success mapping
        return cmdIn.error;                     // Returned error code (1-9)
    }
    return -1; // Timeout Error
}

int32_t HoverSerial::getParam(HoverboardParam_t param) {
    emptyBuffer(); // Protect against hardware buffer overflows
    HoverboardSettingsCommand cmdOut, cmdIn;
    Hoverboard_BuildSettings(&cmdOut, CMD_GET, param, 0);
    _serial->write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 500)) {
        if (cmdIn.error == ERR_OK) return cmdIn.param_value;
    }
    return 0; // Return 0 on timeout or error
}

int8_t HoverSerial::resetParam(HoverboardParam_t param) {
    emptyBuffer(); 
    HoverboardSettingsCommand cmdOut, cmdIn;
    Hoverboard_BuildSettings(&cmdOut, CMD_INIT, param, 0);
    _serial->write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 500)) { 
        if (cmdIn.error == ERR_OK) return 0;    
        return cmdIn.error;                     
    }
    return -1; // Timeout Error
}

int8_t HoverSerial::eepromSave() {
    emptyBuffer(); 
    HoverboardSettingsCommand cmdOut, cmdIn;
    // Parameter index and value are ignored for SAVE command, so we pass 0
    Hoverboard_BuildSettings(&cmdOut, CMD_SAVE, (HoverboardParam_t)0, 0);
    _serial->write((uint8_t*)&cmdOut, sizeof(cmdOut));

    if (waitForSettingsResponse(&cmdIn, 500)) { 
        if (cmdIn.error == ERR_OK) return 0;    
        return cmdIn.error;                     
    }
    return -1; // Timeout Error
}

HoverFeedback HoverSerial::getTelemetry() {
    // Continuously chew through the buffer to guarantee the freshest data is returned
    while (_serial->available()) {
        _telemetry_buffer[_telemetry_bufIdx++] = _serial->read();
        if (_telemetry_bufIdx == sizeof(HoverFeedback)) {
            HoverFeedback *fb = (HoverFeedback *)_telemetry_buffer;
            uint16_t sum = 0;
            for (size_t i = 0; i < sizeof(HoverFeedback) - 2; i++) sum += _telemetry_buffer[i];
            if (fb->start == SERIAL_START_FRAME && sum == fb->checksum) {
                memcpy(&_lastFeedback, _telemetry_buffer, sizeof(HoverFeedback));
                _telemetry_bufIdx = 0; // Reset index to catch the next valid frame
            } else {
                memmove(_telemetry_buffer, _telemetry_buffer + 1, sizeof(HoverFeedback) - 1); // Shift on fail
                _telemetry_bufIdx--; // Step back to evaluate the next shifted frame
            }
        }
    }
    return _lastFeedback;
}

void HoverSerial::sendCommand(int16_t steer, int16_t speed) {
    HoverboardControlCommand ctrl;
    Hoverboard_BuildControl(&ctrl, steer, speed);
    _serial->write((uint8_t*)&ctrl, sizeof(ctrl));
}
#endif