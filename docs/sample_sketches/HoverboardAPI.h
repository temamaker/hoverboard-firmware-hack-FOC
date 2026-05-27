#ifndef HOVERBOARD_API_H
#define HOVERBOARD_API_H

#include <stdint.h>
#include <stddef.h>

// Magic Numbers
#define SERIAL_START_FRAME    0xABCD
#define SERIAL_SETTINGS_FRAME 0x1234

// Command Indices
typedef enum {
    CMD_GET   = 0,
    CMD_HELP  = 1,
    CMD_WATCH = 2,
    CMD_SET   = 3,
    CMD_INIT  = 4,
    CMD_SAVE  = 5
} HoverboardCommand_t;

// Error Codes
typedef enum {
    ERR_OK                     = -1,
    ERR_CMD_NOT_FOUND          = 1,
    ERR_PARAM_NOT_FOUND        = 2,
    ERR_CANNOT_WRITE_VARIABLE  = 3,
    ERR_VALUE_OUT_OF_BOUNDS    = 4,
    ERR_VALUE_EXPECTED         = 5,
    ERR_PARAM_EXPECTED         = 8
} HoverboardError_t;

// Parameter Index Map (Extracted from Hoverboard Firmware)
typedef enum {
    PARAM_CTRL_MOD    = 0,
    PARAM_CTRL_TYP    = 1,
    PARAM_I_MOT_MAX   = 2,
    PARAM_N_MOT_MAX   = 3,
    PARAM_FI_WEAK_ENA = 4,
    PARAM_FI_WEAK_HI  = 5,
    PARAM_FI_WEAK_LO  = 6,
    PARAM_FI_WEAK_MAX = 7,
    PARAM_PHA_ADV_MAX = 8,
    VAR_IN1_RAW       = 9,
    PARAM_IN1_TYP     = 10,
    PARAM_IN1_MIN     = 11,
    PARAM_IN1_MID     = 12,
    PARAM_IN1_MAX     = 13,
    VAR_IN1_CMD       = 14,
    VAR_IN2_RAW       = 15,
    PARAM_IN2_TYP     = 16,
    PARAM_IN2_MIN     = 17,
    PARAM_IN2_MID     = 18,
    PARAM_IN2_MAX     = 19,
    VAR_IN2_CMD       = 20,
    VAR_DC_CURR       = 21,
    VAR_RDC_CURR      = 22,
    VAR_LDC_CURR      = 23,
    VAR_CMDL          = 24,
    VAR_CMDR          = 25,
    VAR_SPD_AVG       = 26,
    VAR_SPDL          = 27,
    VAR_SPDR          = 28,
    VAR_RATE          = 29,
    VAR_SPD_COEF      = 30,
    VAR_STR_COEF      = 31,
    VAR_BATV          = 32,
    VAR_TEMP          = 33
} HoverboardParam_t;

// --- Parameter Value Constants ---
typedef enum {
    OPEN_MODE = 0,
    VLT_MODE  = 1,
    SPD_MODE  = 2,
    TRQ_MODE  = 3
} HoverboardCtrlMod_t;

typedef enum {
    COM_CTRL = 0,
    SIN_CTRL = 1,
    FOC_CTRL = 2
} HoverboardCtrlTyp_t;

typedef enum {
    INPUT_DISABLED   = 0,
    INPUT_NORMAL_POT = 1,
    INPUT_MID_POT    = 2,
    INPUT_AUTO       = 3
} HoverboardInputTyp_t;

typedef enum {
    FIELD_WEAK_OFF = 0,
    FIELD_WEAK_ON  = 1
} HoverboardFieldWeak_t;

// Binary Structs
// Using __attribute__((packed)) forces the ESP32 to format this exactly 
// the same way as the STM32, bypassing 32-bit architectural padding!

typedef struct {
    uint16_t start;
    int16_t  steer;
    int16_t  speed;
    uint16_t checksum;
} __attribute__((packed)) HoverboardControlCommand;

typedef struct {
    uint16_t packet_start;
    uint8_t  semaphore;
    int8_t   error;
    int8_t   command_index;
    int8_t   param_index;
    int32_t  param_value;
    uint16_t checksum;
} __attribute__((packed)) HoverboardSettingsCommand;

#ifdef __cplusplus
extern "C" {
#endif

void Hoverboard_BuildControl(HoverboardControlCommand *cmd, int16_t steer, int16_t speed);
void Hoverboard_BuildSettings(HoverboardSettingsCommand *cmd, HoverboardCommand_t action, HoverboardParam_t param, int32_t value);

typedef struct {
    uint16_t start;
    int16_t  ctrl_mod;
    int16_t  ctrl_typ;
    int16_t  i_mot_max;
    int16_t  n_mot_max;
    int16_t  fi_weak_ena;
    int16_t  fi_weak_hi;
    int16_t  fi_weak_lo;
    int16_t  fi_weak_max;
    int16_t  pha_adv_max;
    int16_t  in1_raw;
    int16_t  in1_typ;
    int16_t  in1_min;
    int16_t  in1_mid;
    int16_t  in1_max;
    int16_t  in1_cmd;
    int16_t  in2_raw;
    int16_t  in2_typ;
    int16_t  in2_min;
    int16_t  in2_mid;
    int16_t  in2_max;
    int16_t  in2_cmd;
    int16_t  dc_curr;
    int16_t  rdc_curr;
    int16_t  ldc_curr;
    int16_t  cmdl;
    int16_t  cmdr;
    int16_t  spd_avg;
    int16_t  spdl;
    int16_t  spdr;
    int16_t  rate;
    int16_t  spd_coef;
    int16_t  str_coef;
    int16_t  batv;
    int16_t  temp;
    uint16_t checksum;
} __attribute__((packed)) HoverboardFeedback;

// Alias to match pseudocode simplicity
typedef HoverboardFeedback HoverFeedback;

#ifdef __cplusplus
}

#include <Arduino.h>

class HoverSerial {
  private:
    HardwareSerial* _serial;
    HoverFeedback _lastFeedback;
    
    // Independent state machine memory so you can control multiple hoverboards!
    uint8_t _telemetry_buffer[sizeof(HoverFeedback)];
    int _telemetry_bufIdx;

    void emptyBuffer();
    bool waitForSettingsResponse(HoverboardSettingsCommand *response, uint32_t timeout_ms);

  public:
    HoverSerial(HardwareSerial& serial);
    
    // Optional pin definitions for ESP32 routing
    void start(unsigned long baudrate, int8_t rx_pin = -1, int8_t tx_pin = -1);
    
    int8_t setParam(HoverboardParam_t param, int32_t value);
    int32_t getParam(HoverboardParam_t param);
    int8_t resetParam(HoverboardParam_t param);
    int8_t eepromSave();
    HoverFeedback getTelemetry();
    void sendCommand(int16_t steer, int16_t speed);
};

#endif

#endif // HOVERBOARD_API_H