#include <Arduino.h>
#include "HoverboardAPI.h"

// Instantiate the hoverboard object and map it to Serial2
HoverSerial roverComms(Serial2);

void setup() {
    Serial.begin(115200);
    
    // ESP32 Serial2 natively routes to pins 16(RX) and 17(TX), but you can pass them as arguments if needed!
    roverComms.start(115200); 
    
    delay(3000); // Give the user time to open the serial monitor
    Serial.println("\n\n=== Hoverboard Binary Protocol Test Rig ===");
    
    if (roverComms.setParam(PARAM_N_MOT_MAX, 1500) != 0) {
        Serial.println("There has been an error setting the parameter!");
    }
}

void loop() {
    HoverFeedback telemetry = roverComms.getTelemetry();

    Serial.print("Battery: ");
    Serial.print(telemetry.batv / 100.0);
    Serial.println("V");

    int steer = 0; 
    int speed = 100; // Small constant speed to verify operation
    roverComms.sendCommand(steer, speed);
    
    delay(20); // Required! Prevents the ESP32 from spamming the hoverboard too fast
}