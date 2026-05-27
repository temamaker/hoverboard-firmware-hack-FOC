# HoverboardAPI C++ Library

The `HoverboardAPI` library is a lightweight, non-blocking C++ wrapper designed to make communication with your STM32 Hoverboard incredibly simple. It completely abstracts away the complex binary protocols, checksum calculations, and UART hardware management.

If you can call a function, you can build a robot!

---

## 1. Quick Start Guide

To use this library in your ESP32 / Arduino project:
1. Copy `HoverboardAPI.h` and `HoverboardAPI.cpp` into your project folder.
2. Include the header at the top of your main sketch.
3. Instantiate the `HoverSerial` object by passing it a Hardware Serial port.

### Basic Example
```cpp
#include <Arduino.h>
#include "HoverboardAPI.h"

// 1. Create the hoverboard object using Serial2
HoverSerial roverComms(Serial2);

void setup() {
    Serial.begin(115200);
    
    // 2. Start the hoverboard serial connection at 115200 baud
    // (Optional: You can pass custom RX/TX pins like roverComms.start(115200, 16, 17))
    roverComms.start(115200); 
    
    delay(2000);

    // 3. Setup a parameter safely!
    if (roverComms.setParam(PARAM_N_MOT_MAX, 1500) != 0) {
        Serial.println("Error configuring the hoverboard!");
    }
    
    // 4. Change Control Type using the safe human-readable Enums!
    roverComms.setParam(PARAM_CTRL_TYP, FOC_CTRL);
    roverComms.setParam(PARAM_CTRL_MOD, VLT_MODE);

    // 5. Read back a parameter
    int32_t currentMaxRpm = roverComms.getParam(PARAM_N_MOT_MAX);
    Serial.print("Confirmed max RPM is: ");
    Serial.println(currentMaxRpm);

    // 6. Save parameters to EEPROM so they persist after reboot!
    // (NOTE: The hoverboard will crash if you try to save while the wheels are moving)
    roverComms.eepromSave();
}

void loop() {
    // 5. Read the live telemetry!
    HoverFeedback telemetry = roverComms.getTelemetry();

    Serial.print("Battery Voltage: ");
    Serial.print(telemetry.batv / 100.0); // Converted from hundredths
    Serial.println("V");

    // 6. Send a driving command
    int steer = 0; 
    int speed = 200; 
    roverComms.sendCommand(steer, speed);
    
    // 7. Crucial loop delay! (Read Best Practices below)
    delay(20); 
}
```

---

## 2. API Reference

### `HoverSerial::start(baudrate, [rx_pin], [tx_pin])`
Initializes the hardware serial port. 
* **`baudrate`**: Must match the STM32 firmware (Usually `115200` or `230400`).
* **`rx_pin`, `tx_pin`** *(Optional)*: Allows you to remap the default hardware serial pins on microcontrollers like the ESP32.

### `HoverSerial::sendCommand(steer, speed)`
Sends a real-time high-frequency driving command to the motors.
* **`steer`**: Integer between `-1000` (Full Left) and `1000` (Full Right).
* **`speed`**: Integer between `-1000` (Full Reverse) and `1000` (Full Forward).
* **Behavior:** Non-blocking. Executes in under 1 millisecond.

### `HoverSerial::getTelemetry()`
Retrieves the freshest variables and statistics from the hoverboard.
* **Returns:** A `HoverFeedback` struct containing 34 variables (e.g., `batv`, `dc_curr`, `spd_avg`, `temp`).
* **Behavior:** Non-blocking. It automatically eats through the UART hardware buffer and retrieves the most recent valid packet, discarding old/stagnant data to prevent latency lag!

### `HoverSerial::setParam(param, value)`
Changes a setting on the hoverboard and waits for a confirmation that the command was successfully executed.
* **`param`**: The target index. (Use the `HoverboardParam_t` definitions).
* **`value`**: The integer value to write. (Use the built-in Enums for magic numbers!).
* **Returns:** 
  * `0` = Success!
  * `1 to 9` = Hoverboard rejected the command (Error Code).
  * `-1` = Timeout (The hoverboard never responded).
* **Behavior:** **Blocking**. This function will halt your main loop for up to 500ms while waiting for the STM32 to acknowledge the change. 

### `HoverSerial::getParam(param)`
Retrieves the current configured value of a specific parameter.
* **`param`**: The target index. (Use the `HoverboardParam_t` definitions).
* **Returns:** The `int32_t` parameter value. (Returns `0` if a timeout or error occurs).
* **Behavior:** **Blocking** for up to 500ms.

### `HoverSerial::resetParam(param)`
Restores a specific parameter back to its original default firmware setting.
* **`param`**: The target index. (Use the `HoverboardParam_t` definitions).
* **Returns:** Same error codes as `setParam()`.
* **Behavior:** **Blocking** for up to 500ms.

### `HoverSerial::eepromSave()`
Permanently saves all active parameters to the hoverboard's internal Flash/EEPROM memory so they persist after turning the board off!
* **Returns:** Same error codes as `setParam()`.
  * *Note: Returns Error 9 if you attempt to save while the motors are spinning.*
* **Behavior:** **Blocking** for up to 500ms. 
* **Warning:** Never execute this function while the wheels are moving! Writing to Flash memory halts the STM32 processor, which will cause the robot to jerk and crash.

---

## 3. Human-Readable Value Enums
To prevent users from accidentally sending invalid parameter states, the library includes safe definitions.

### Control Mode (`PARAM_CTRL_MOD`)
* `OPEN_MODE` (0)
* `VLT_MODE` (1) - Voltage (Standard driving)
* `SPD_MODE` (2) - Speed (Cruise control)
* `TRQ_MODE` (3) - Torque (Skateboard/freewheel style)

### Control Type (`PARAM_CTRL_TYP`)
* `COM_CTRL` (0) - Commutation
* `SIN_CTRL` (1) - Sinusoidal
* `FOC_CTRL` (2) - Field Oriented Control (Smoothest & Quietest)

### Input Types (`PARAM_IN1_TYP`, `PARAM_IN2_TYP`)
* `INPUT_DISABLED` (0)
* `INPUT_NORMAL_POT` (1)
* `INPUT_MID_POT` (2) - Joystick / resting-center potentiometer
* `INPUT_AUTO` (3)

### Field Weakening (`PARAM_FI_WEAK_ENA`)
* `FIELD_WEAK_OFF` (0)
* `FIELD_WEAK_ON` (1)

---

## 4. Best Practices & Caveats ⚠️

To keep your robot stable and safe, strictly observe the following rules:

#### 1. Always include a small delay in your main loop.
Your microcontroller (like an ESP32) can run millions of times per second. If you spam `sendCommand()` as fast as possible, you will instantly overflow the STM32 Hoverboard's UART buffer and crash it!
* **Do:** Add a `delay(20);` at the end of your `loop()` to run at 50Hz.

#### 2. Do not use huge delays (`delay(500)`) while driving!
The hoverboard has a built-in safety timeout. If it does not receive a `sendCommand` for a few hundred milliseconds, it will trigger a "General Timeout" (Beeping 4 times) and shut off the motors to prevent runaway robots!
* **Do:** Ensure your loop executes at least 10 times a second (100ms maximum delay).

#### 3. You MUST call `getTelemetry()` constantly!
The hoverboard transmits a 72-byte telemetry packet 50 times per second. Your microcontroller's hardware UART buffer only holds 256 bytes. If you don't read it, the buffer will overflow in exactly 70 milliseconds!
* **Do:** Call `getTelemetry()` every loop cycle to keep the hardware buffer empty and ensure you always have zero-latency data.

#### 4. Never use `setparam` inside a high-speed driving loop.
`setParam()` and `eepromSave()` are blocking functions. They force your microcontroller to stop and wait for a response. Furthermore, if you save settings while the wheels are moving, it stalls the flash memory and crashes the hoverboard!
* **Do:** Only use `setParam()` in your `setup()` function, or when the robot is completely stopped.

---

## 5. Struct Variables Reference
When calling `roverComms.getTelemetry()`, you gain access to the following variables:

| Code Name | Description | Conversion required |
| :--- | :--- | :--- |
| `.batv` | Battery Voltage | Divide by 100.0 (e.g. 3850 = 38.5V) |
| `.temp` | Motherboard Temp | Divide by 10.0 (e.g. 355 = 35.5°C) |
| `.spd_avg` | Average Wheel RPM | None |
| `.spdl` / `.spdr` | Individual Wheel RPM | None |
| `.dc_curr` | Total Power Draw | Divide by 100.0 (Amps) |
| `.cmdl` / `.cmdr` | Target Internal Commd | None |

*To view all 34 available variables, open the `HoverboardAPI.h` file and inspect the `HoverFeedback` struct!*