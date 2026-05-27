#include <Arduino.h>
#include "HoverboardAPI.h"

// Initialize Hoverboard communication on Serial2
HoverSerial roverComms(Serial2);

int current_speed = 0;
uint32_t lastPrintTime = 0;

void printInstructions() {
    Serial.println("\n--- Hoverboard Interactive Terminal ---");
    Serial.println("  U : Increase Speed (+5%)");
    Serial.println("  D : Decrease Speed (-5%)");
    Serial.println("  V : Set VOLTAGE Mode");
    Serial.println("  S : Set SPEED Mode");
    Serial.println("  T : Set TORQUE Mode");
    Serial.println("  o : Set OPEN Mode");
    Serial.println("  f : Set FOC Control");
    Serial.println("  s : Set SIN Control");
    Serial.println("  c : Set COM Control");
    Serial.println("  W : Toggle Field Weakening");
    Serial.println("---------------------------------------\n");
}

void setup() {
    Serial.begin(115200);
    roverComms.start(115200);
    
    delay(2000);
    printInstructions();
}

void loop() {
    // 1. Always grab the latest telemetry to keep the hardware buffer empty!
    HoverFeedback telemetry = roverComms.getTelemetry();

    // 2. Process incoming commands from the Serial Monitor
    while (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'U': 
                current_speed = min(1000, current_speed + 50); // +5% of 1000
                break;
            case 'D': 
                current_speed = max(-1000, current_speed - 50); // -5% of 1000
                break;
            case 'V': 
                roverComms.setparam(PARAM_CTRL_MOD, VLT_MODE);
                break;
            case 'S': 
                roverComms.setparam(PARAM_CTRL_MOD, SPD_MODE);
                break;
            case 'T': 
                roverComms.setparam(PARAM_CTRL_MOD, TRQ_MODE);
                break;
            case 'o': 
                roverComms.setparam(PARAM_CTRL_MOD, OPEN_MODE);
                break;
            case 'f': 
                roverComms.setparam(PARAM_CTRL_TYP, FOC_CTRL);
                break;
            case 's': 
                roverComms.setparam(PARAM_CTRL_TYP, SIN_CTRL);
                break;
            case 'c': 
                roverComms.setparam(PARAM_CTRL_TYP, COM_CTRL);
                break;
            case 'W': 
                // Toggle based on the actual live telemetry reading!
                if (telemetry.fi_weak_ena == FIELD_WEAK_ON) {
                    roverComms.setparam(PARAM_FI_WEAK_ENA, FIELD_WEAK_OFF);
                } else {
                    roverComms.setparam(PARAM_FI_WEAK_ENA, FIELD_WEAK_ON);
                }
                break;
        }
    }

    // 3. Send the constant 50Hz driving command
    roverComms.sendCommand(0, current_speed);

    // 4. Print a live dashboard every 500ms
    if (millis() - lastPrintTime > 500) {
        // Map the numeric mode parameters to readable strings
        String ctrl_mod_str = (telemetry.ctrl_mod == 0) ? "OPEN" : (telemetry.ctrl_mod == 1) ? "VOLTAGE" : (telemetry.ctrl_mod == 2) ? "SPEED" : "TORQUE";
        String ctrl_typ_str = (telemetry.ctrl_typ == 0) ? "COM" : (telemetry.ctrl_typ == 1) ? "SIN" : "FOC";
        String fw_str = (telemetry.fi_weak_ena == 1) ? "ON" : "OFF";

        Serial.println("=========================================");
        Serial.printf(" CMD Speed : %d (%.1f%%)\n", current_speed, (current_speed / 1000.0) * 100.0);
        Serial.println("--- Core Telemetry ---");
        Serial.printf(" Mode      : %s (%s)\n", ctrl_typ_str.c_str(), ctrl_mod_str.c_str());
        Serial.printf(" F.Weak    : %s\n", fw_str.c_str());
        Serial.printf(" Battery   : %.2f V\n", telemetry.batv / 100.0);
        Serial.printf(" Temp      : %.1f C\n", telemetry.temp / 10.0);
        Serial.printf(" DC Current: %.2f A\n", telemetry.dc_curr / 100.0);
        Serial.printf(" Speed Avg : %d RPM\n", telemetry.spd_avg);
        Serial.printf(" N_MOT_MAX : %d RPM\n", telemetry.n_mot_max);
        Serial.printf(" I_MOT_MAX : %d A\n", telemetry.i_mot_max);
        Serial.printf(" CMD_L     : %d\n", telemetry.cmdl);
        Serial.printf(" CMD_R     : %d\n", telemetry.cmdr);
        Serial.println("=========================================\n");
        
        lastPrintTime = millis();
    }

    // 5. Hard loop delay to prevent UART flooding (50Hz)
    delay(20);
}