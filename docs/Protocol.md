# Hoverboard Binary Communication Protocol

This document defines the bidirectional binary communication protocol between the Hoverboard (STM32) and the Master Controller (e.g., ESP32). 

The physical layer uses standard UART (115200 baud, 8N1). To safely multiplex high-frequency control data and low-frequency system configuration over a single UART port, the protocol uses distinct **Magic Numbers** (Start Frames).

## 1. Magic Numbers (Start Frames)

| Frame Name | Magic Number | Purpose |
| :--- | :--- | :--- |
| `SERIAL_START_FRAME` | **`0xABCD`** | Used for high-frequency **Control** (Steer/Speed) and **Telemetry** (Stats). |
| `SERIAL_SETTINGS_FRAME` | **`0x1234`** | Used for low-frequency **Settings** (Read/Write Parameters). |

---

## 2. Control Packet (Master ➜ Hoverboard)
Used to send real-time driving commands to the hoverboard.
* **Start Frame:** `0xABCD`
* **Size:** 8 bytes
* **Checksum:** XOR sum of all 16-bit integers preceding the checksum.

| Offset | Type | Name | Description |
| :--- | :--- | :--- | :--- |
| 0 | `uint16_t` | `start` | `0xABCD` |
| 2 | `int16_t` | `steer` | Steering command (-1000 to 1000) |
| 4 | `int16_t` | `speed` | Speed command (-1000 to 1000) |
| 6 | `uint16_t` | `checksum` | `start ^ steer ^ speed` |

---

## 3. Telemetry / Stats Packet (Hoverboard ➜ Master)
Used to stream high-frequency variables and states to the master.
* **Start Frame:** `0xABCD`
* **Size:** 72 bytes (Strictly packed)
* **Rate:** 50 Hz (20ms interval)
* **Checksum:** Mathematical sum of all preceding bytes (Bytes 0 to 69).

| Offset | Type | Name | Description |
| :--- | :--- | :--- | :--- |
| 0 | `uint16_t` | `start` | `0xABCD` |
| 2 | `int16_t` | `ctrl_mod` | Ctrl mode 1:Voltage 2:Speed 3:Torque |
| 4 | `int16_t` | `ctrl_typ` | Ctrl type 0:Commutation 1:Sinusoidal 2:FOC |
| 6 | `int16_t` | `i_mot_max` | Max phase current A |
| 8 | `int16_t` | `n_mot_max` | Max motor RPM |
| 10 | `int16_t` | `fi_weak_ena` | Enable field weak 0:OFF 1:ON |
| 12 | `int16_t` | `fi_weak_hi` | Field weak high RPM |
| 14 | `int16_t` | `fi_weak_lo` | Field weak low RPM |
| 16 | `int16_t` | `fi_weak_max` | Field weak max current A(FOC only) |
| 18 | `int16_t` | `pha_adv_max` | Max Phase Adv angle Deg(SIN only) |
| 20 | `int16_t` | `in1_raw` | Input1 raw value |
| 22 | `int16_t` | `in1_typ` | Input1 type |
| 24 | `int16_t` | `in1_min` | Input1 minimum value |
| 26 | `int16_t` | `in1_mid` | Input1 middle value |
| 28 | `int16_t` | `in1_max` | Input1 maximum value |
| 30 | `int16_t` | `in1_cmd` | Input1 command value |
| 32 | `int16_t` | `in2_raw` | Input2 raw value |
| 34 | `int16_t` | `in2_typ` | Input2 type |
| 36 | `int16_t` | `in2_min` | Input2 minimum value |
| 38 | `int16_t` | `in2_mid` | Input2 middle value |
| 40 | `int16_t` | `in2_max` | Input2 maximum value |
| 42 | `int16_t` | `in2_cmd` | Input2 command value |
| 44 | `int16_t` | `dc_curr` | Total DC Link current A *100 |
| 46 | `int16_t` | `rdc_curr` | Right DC Link current A *100 |
| 48 | `int16_t` | `ldc_curr` | Left DC Link current A *100 |
| 50 | `int16_t` | `cmdl` | Left Motor Command RPM |
| 52 | `int16_t` | `cmdr` | Right Motor Command RPM |
| 54 | `int16_t` | `spd_avg` | Motor Measured Avg RPM |
| 56 | `int16_t` | `spdl` | Left Motor Measured RPM |
| 58 | `int16_t` | `spdr` | Right Motor Measured RPM |
| 60 | `int16_t` | `rate` | Rate *10 |
| 62 | `int16_t` | `spd_coef` | Speed Coefficient *10 |
| 64 | `int16_t` | `str_coef` | Steer Coefficient *10 |
| 66 | `int16_t` | `batv` | Calibrated Battery Voltage *100 |
| 68 | `int16_t` | `temp` | Calibrated Temperature °C *10 |
| 70 | `uint16_t` | `checksum` | Sum of Bytes `0` through `69`. |

---

## 4. Settings Packet (Bidirectional)
Used to Read (GET), Write (SET), and SAVE variables and parameters.
* **Start Frame:** `0x1234`
* **Size:** 12 bytes (Strictly packed)
* **Checksum:** Mathematical sum of all preceding bytes (Bytes 0 to 9).

| Offset | Type | Name | Description |
| :--- | :--- | :--- | :--- |
| 0 | `uint16_t` | `packet_start` | `0x1234` |
| 2 | `uint8_t` | `semaphore` | Handshake flag. (Usually 0 from Master) |
| 3 | `int8_t` | `error` | Status code. `-1` = OK. `1-9` = Error Codes. |
| 4 | `int8_t` | `command_index`| Action to perform. (See Command Dictionary) |
| 5 | `int8_t` | `param_index` | Target variable ID. (See Parameter Dictionary) |
| 6 | `int32_t` | `param_value` | The payload value being written or returned. |
| 10 | `uint16_t` | `checksum` | Sum of Bytes `0` through `9`. |

---

## 5. Dictionaries

### Command Index (`command_index`)
| ID | Name | Description |
| :--- | :--- | :--- |
| 0 | GET | Reads a parameter. Returns the value in `param_value`. |
| 3 | SET | Writes `param_value` to the parameter index. |
| 4 | INIT | Resets parameter to EEPROM or Config.h defaults. |
| 5 | SAVE | Flushes all current parameters to non-volatile EEPROM. |

### Error Codes (`error`)
| Code | Description |
| :--- | :--- |
| -1 | Success / OK |
| 1 | Command Not Found |
| 2 | Parameter Index Not Found |
| 3 | Access Denied: Cannot WRITE to a Read-Only VARIABLE |
| 4 | Value out of bounds (Min/Max violation) |
| 8 | Parameter Expected |

### Parameter Index (`param_index`)
*A selection of core variables. Refer to C-header for full mapping.*

| ID | Type | Name | Bounds/Notes |
| :--- | :--- | :--- | :--- |
| 0 | Param | `CTRL_MOD` | 1: Voltage, 2: Speed, 3: Torque |
| 1 | Param | `CTRL_TYP` | 0: Commutation, 1: Sinusoidal, 2: FOC |
| 2 | Param | `I_MOT_MAX` | Max Phase Current (Amps) |
| 3 | Param | `N_MOT_MAX` | Max Motor RPM |
| 4 | Param | `FI_WEAK_ENA` | Field Weakening Enable (0: OFF, 1: ON) |
| 9 | Var | `IN1_RAW` | Read-Only. Input 1 Raw Value. |
| ... | ... | ... | ... |

> **Note on Variable vs Parameter:** Parameters can be modified (SET). Variables represent read-only internal telemetry.