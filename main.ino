/*
 * BMW E87 Instrument Cluster - SimHub Integration
 * 
 * This code allows a BMW E87 (2006 T2036) instrument cluster to work with
 * SimHub for use in racing simulators like Euro Truck Simulator 2 and 
 * American Truck Simulator.
 * 
 * Hardware:
 * - Arduino Uno
 * - CAN-BUS Shield V2 (Seeed Studio)
 * - BMW E87 Instrument Cluster (2006 model, part T2036)
 * - 12V 5A Power Supply for cluster
 * 
 * Wiring:
 * - CAN Shield CS_PIN: 9
 * - CAN Shield IRQ_PIN: 2
 * - CAN-H and CAN-L connected to cluster
 * - Common ground between Arduino, cluster, and power supply
 * - IMPORTANT: Cut the termination resistor trace (P1) on the CAN shield
 * 
 * Library Required:
 * - CAN by Sandeep Mistry (install via Arduino Library Manager)
 *   NOT the MCP_CAN library!
 * 
 * SimHub Configuration:
 * - Baud rate: 115200
 * - Update message format: JavaScript
 * - See JavaScript code in comments below
 * 
 * Features Working:
 * - RPM gauge
 * - Speedometer (km/h on inner scale)
 * - Fuel gauge
 * - Turn indicators with sound
 * - Headlights, high beams, fog lights
 * - Handbrake warning light
 * - Ignition control (cluster on/off)
 * - Warning lights (ABS, Oil, EML, Steering) - all OFF
 * - Engine temperature (displayed on LCD)
 * 
 * Author: Community effort
 * Original inspiration: TeksuSiK/e87-cluster-simhub
 * CAN protocol reference: loopybunny.co.uk/CarPC/k_can.html
 * 
 * License: MIT
 * 
 * SimHub JavaScript (paste in Custom Serial Device):
 * 
 * var ignition = $prop('EngineIgnitionOn');
 * var engine_running = $prop('EngineStarted');
 * var lights_side = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Parking'));
 * var lights_dip = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamLow'));
 * var lights_main = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamHigh'));
 * var lights_front_fog = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Beacon'));
 * var lights_rear_fog = 0;
 * var lights_indicators;
 * var lights_indicator_left_prop = $prop('GameRawData.TruckValues.CurrentValues.LightsValues.BlinkerLeftOn');
 * var lights_indicator_right_prop = $prop('GameRawData.TruckValues.CurrentValues.LightsValues.BlinkerRightOn');
 * if (lights_indicator_left_prop && lights_indicator_right_prop) {
 *     lights_indicators = 3;
 * } else if (lights_indicator_left_prop) {
 *     lights_indicators = 1;
 * } else if (lights_indicator_right_prop) {
 *     lights_indicators = 2;
 * } else {
 *     lights_indicators = 0;
 * }
 * var rpm = $prop('Rpms');
 * var speed = $prop('SpeedKmh');
 * var currentFuel = $prop('Fuel');
 * var maxFuel = $prop('MaxFuel');
 * function calculate(current, max) {
 *     return (current / max) * 100;
 * }
 * var fuel = calculate(currentFuel, maxFuel);
 * if (isNaN(fuel)) {
 *     fuel = 0;
 * }
 * var engine_temperature = $prop('GameRawData.TruckValues.CurrentValues.DashboardValues.WaterTemperature');
 * var handbrake = Number($prop('GameRawData.TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake'));
 * var abs = 0;
 * var airbag = 0;
 * var seatbelt = 0;
 * var currentTime = new Date();
 * var hour = currentTime.getHours();
 * var minute = currentTime.getMinutes();
 * var second = currentTime.getSeconds();
 * var year = currentTime.getFullYear();
 * var month = currentTime.getMonth();
 * var day = currentTime.getDay();
 * return `SH;${ignition};${engine_running};${lights_side};${lights_dip};${lights_main};${lights_front_fog};${lights_rear_fog};${lights_indicators};${rpm};${speed};${fuel};${engine_temperature};${handbrake};${abs};${airbag};${seatbelt};${hour};${minute};${second};${day};${month};${year};\n`;
 * 
 */

#include <CAN.h>

// Macro helpers for splitting 16-bit values into high/low bytes
#define lo8(x) (uint8_t)((x)&0xff)
#define hi8(x) (uint8_t)(((x) >> 8) & 0xff)

// CAN Shield pin configuration
const uint8_t CS_PIN = 9;   // Chip Select pin
const uint8_t IRQ_PIN = 2;  // Interrupt pin

// Serial communication buffers
String inputString = "";     // Buffer for incoming serial data
String fields[30];          // Parsed data fields from SimHub
int fieldCount = 0;         // Number of fields received

// ===== Vehicle State Variables =====
// These are updated from SimHub data
uint16_t g_rpm = 800;                    // Engine RPM
uint16_t g_speed = 0;                    // Speed in km/h
uint16_t g_fuel = 50;                    // Fuel percentage (0-100)
uint8_t g_lights_indicators = 0;         // 0=off, 1=left, 2=right, 3=hazard
uint16_t g_engine_temperature = 90;      // Engine temperature in Celsius
bool g_handbrake = false;                // Handbrake engaged

// Ignition states
bool g_ignition = false;                 // Key in ignition (position 1)
bool g_engine_running = false;           // Engine running (position 2)

// Light states
bool g_lights_side = false;              // Parking/side lights
bool g_lights_dip = false;               // Low beam headlights
bool g_lights_main = false;              // High beam headlights
bool g_lights_front_fog = false;         // Front fog lights
bool g_lights_rear_fog = false;          // Rear fog lights

// ===== Light Bit Definitions for CAN ID 0x21A =====
#define SIDE 0x01        // Parking lights bit
#define DIP 0x02         // Low beam bit
#define MAIN 0x04        // High beam bit
#define FRONT_FOG 0x10   // Front fog lights bit
#define REAR_FOG 0x20    // Rear fog lights bit

// ===== CAN Message Functions =====

/*
 * CAN ID: 0x130
 * Purpose: Main ignition control and cluster wake/sleep
 * Frequency: 100ms
 * 
 * Byte 0: Ignition state
 *   0x00 = Everything off
 *   0x41 = Key in position 1 (ignition on, engine off)
 *   0x45 = Engine running (position 2)
 * Byte 1: 0x40 = Key in slot
 * Byte 2: 0x21 = Clutch/running state
 * Byte 3: 0x8F = Running state flags
 * Byte 4: Counter (increments every call)
 */
void sendIgnitionKeyOn() {
  static uint8_t counter = 0;
  uint8_t byte0 = 0x00;
  
  if (g_engine_running) {
    byte0 = 0x45;  // Engine running
  } else if (g_ignition) {
    byte0 = 0x41;  // Ignition on, engine off
  } else {
    byte0 = 0x00;  // Everything off
  }
  
  CAN.beginPacket(0x130);
  CAN.write(byte0);
  CAN.write(0x40);
  CAN.write(0x21);
  CAN.write(0x8F);
  CAN.write(counter);
  CAN.endPacket();
  
  counter++;
}

/*
 * CAN ID: 0x26E
 * Purpose: Ignition position display (works with 0x130)
 * Frequency: 100ms
 * 
 * Determines what the cluster displays based on ignition state
 */
void sendIgnitionStatus() {
  uint8_t byte0, byte1, byte2;
  
  if (g_engine_running) {
    // Position 2: Engine running
    byte0 = 0x40;
    byte1 = 0x40;
    byte2 = 0x7F;
  } else if (g_ignition) {
    // Position 1: Ignition on
    byte0 = 0x00;
    byte1 = 0x40;
    byte2 = 0x7F;
  } else {
    // Position 0: Everything off
    byte0 = 0x00;
    byte1 = 0x00;
    byte2 = 0x3F;
  }
  
  CAN.beginPacket(0x26E);
  CAN.write(byte0);
  CAN.write(byte1);
  CAN.write(byte2);
  CAN.write(0x50);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.endPacket();
}

/*
 * CAN ID: 0x0AA
 * Purpose: Engine RPM display
 * Frequency: 100ms
 * 
 * RPM value is multiplied by 4 before sending
 * Example: 2000 RPM → send 8000
 */
void sendRPM(uint16_t rpm) {
  uint16_t tempRpm = rpm * 4;
  
  CAN.beginPacket(0x0AA);
  CAN.write(0xFE);
  CAN.write(0xFE);
  CAN.write(0xFF);
  CAN.write(0x00);
  CAN.write(lo8(tempRpm));   // RPM low byte
  CAN.write(hi8(tempRpm));   // RPM high byte
  CAN.write(0xFE);
  CAN.write(0x99);
  CAN.endPacket();
}

/*
 * CAN ID: 0x349
 * Purpose: Fuel level display
 * Frequency: 200ms
 * 
 * Converts percentage to sensor value
 * Assumes 70L fuel tank capacity
 * Sensor value = litres * 160
 */
void sendFuelLevel(uint16_t percent) {
  uint16_t litres = (percent * 70) / 100;  // 70L tank
  uint16_t sensor = litres * 160;          // Sensor conversion
  
  CAN.beginPacket(0x349);
  CAN.write(lo8(sensor));
  CAN.write(hi8(sensor));
  CAN.write(lo8(sensor));  // Value repeated 3 times
  CAN.write(hi8(sensor));
  CAN.write(0x00);
  CAN.endPacket();
}

/*
 * CAN ID: 0x1A6
 * Purpose: Vehicle speed and odometer
 * Frequency: 100ms
 * 
 * This is the trickiest message - it's a cumulative counter that increments
 * based on speed and time. The cluster calculates speed from the rate of change.
 * 
 * The speed value is divided by 3 to make the needle point correctly to the
 * km/h inner scale (the cluster expects MPH values).
 * 
 * Bytes 0-5: Speed counter (same value in 3x 16-bit words)
 * Bytes 6-7: Checksum counter (increments by 100 every 50ms)
 */
void sendSpeed(uint16_t speed_kmh) {
  static uint32_t lastTimeSent = 0;
  static uint16_t lastReading = 0;
  static uint16_t counter = 0x0010;
  
  uint32_t now = millis();
  
  // Initialize timing on first call
  if (lastTimeSent == 0) {
    lastTimeSent = now;
    return;
  }
  
  // Divide by 3 to compensate for MPH vs km/h scale
  // This makes the needle point to the correct value on the inner km/h ring
  uint16_t speed_adjusted = speed_kmh / 3;
  
  // Calculate how much time has passed
  uint32_t timeDiff = now - lastTimeSent;
  
  // Formula from loopybunny: (((timeNow - timeLast) / 50) * speed) + lastReading
  uint16_t speedIncrement = ((timeDiff / 50) * speed_adjusted);
  uint16_t speedValue = lastReading + speedIncrement;
  
  lastReading = speedValue;
  lastTimeSent = now;
  
  // Checksum counter increments by 100 every 50ms
  uint16_t counterIncrement = (timeDiff / 50) * 100;
  counter += counterIncrement;
  uint16_t counterMasked = counter & 0x0FFF;  // Only lower 12 bits used
  
  CAN.beginPacket(0x1A6);
  CAN.write(lo8(speedValue));
  CAN.write(hi8(speedValue));
  CAN.write(lo8(speedValue));   // Speed value repeated
  CAN.write(hi8(speedValue));
  CAN.write(lo8(speedValue));   // Speed value repeated
  CAN.write(hi8(speedValue));
  CAN.write(lo8(counterMasked));
  CAN.write(hi8(counterMasked) | 0xF0);  // Upper 4 bits always 0xF
  CAN.endPacket();
}

/*
 * CAN ID: 0x1F6
 * Purpose: Turn signal indicators
 * Frequency: As needed (checked every loop)
 * 
 * Handles blinking timing internally (500ms on/off)
 * Supports left, right, and hazard (both) indicators
 */
void sendIndicators() {
  static uint8_t lastIndicator = 0;
  static uint32_t lastIndicatorTime = 0;
  static uint32_t lastFrameTime = 0;
  
  uint32_t current = millis();
  uint8_t lightIndicator = lastIndicator;
  
  // Handle indicator state changes
  if (g_lights_indicators == 0) {
    if (current - lastIndicatorTime >= 600) {
      lightIndicator = g_lights_indicators;
    }
  } else {
    lightIndicator = g_lights_indicators;
    lastIndicatorTime = current;
  }
  
  // Send message if state changed or 600ms timeout
  if ((lastIndicator != lightIndicator) || (current - lastFrameTime >= 600)) {
    uint8_t byte0, byte1;
    
    if (lightIndicator != 0) {
      // Determine which indicator
      switch (lightIndicator) {
        case 1: byte0 = 0x91; break;  // Left
        case 2: byte0 = 0xA1; break;  // Right
        case 3: byte0 = 0xB1; break;  // Hazard (both)
        default: byte0 = 0x80; break;
      }
      
      // Toggle state for blinking
      if (lastIndicator == lightIndicator) {
        byte1 = 0xF1;
      } else {
        byte1 = 0xF2;
      }
    } else {
      // Indicators off
      byte0 = 0x80;
      byte1 = 0xF0;
    }
    
    lastIndicator = lightIndicator;
    lastFrameTime = current;
    
    CAN.beginPacket(0x1F6);
    CAN.write(byte0);
    CAN.write(byte1);
    CAN.endPacket();
  }
}

/*
 * CAN ID: 0x21A
 * Purpose: Lighting control (headlights, fog lights, etc)
 * Frequency: 100ms
 * 
 * NOTE: The mappings are swapped on this cluster!
 * - g_lights_main controls parking light icon
 * - g_lights_side controls high beam icon
 * This may vary by cluster model.
 */
void sendLights() {
  uint8_t lights = 0x00;
  
  // Build lights byte with bit flags
  if (g_lights_main) lights |= SIDE;           // High beam → parking light bit
  if (g_lights_dip) lights |= DIP;             // Low beam
  if (g_lights_side) lights |= MAIN;           // Parking → high beam bit
  if (g_lights_front_fog) lights |= FRONT_FOG; // Front fog
  if (g_lights_rear_fog) lights |= REAR_FOG;   // Rear fog
  
  CAN.beginPacket(0x21A);
  CAN.write(lights);
  CAN.write(0x00);
  CAN.write(0xF7);
  CAN.endPacket();
}

/*
 * CAN ID: 0x34F
 * Purpose: Handbrake warning light
 * Frequency: 100ms
 * 
 * Byte 0: 0xFE = Handbrake ON, 0xFD = Handbrake OFF
 */
void sendHandbrake() {
  uint8_t handbrake_byte = g_handbrake ? 0xFE : 0xFD;
  
  CAN.beginPacket(0x34F);
  CAN.write(handbrake_byte);
  CAN.write(0xFF);
  CAN.endPacket();
}

/*
 * CAN ID: 0x332
 * Purpose: Oil pressure warning light
 * Frequency: 100ms
 * 
 * Sends 0x00 to keep warning light OFF
 */
void sendOilPressure() {
  CAN.beginPacket(0x332);
  CAN.write(0x00);  // Oil pressure OK
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
}

/*
 * CAN ID: 0x545
 * Purpose: EML/Check Engine warning light
 * Frequency: 100ms
 * 
 * Sends 0xFC to keep warning light OFF
 */
void sendEML() {
  CAN.beginPacket(0x545);
  CAN.write(0xFC);  // Engine OK
  CAN.write(0xFF);
  CAN.endPacket();
}

/*
 * CAN ID: 0x2FC
 * Purpose: Power steering warning light
 * Frequency: 100ms
 * 
 * Sends 0x00 to keep warning light OFF
 */
void sendSteering() {
  CAN.beginPacket(0x2FC);
  CAN.write(0x00);  // Steering OK
  CAN.write(0x00);
  CAN.endPacket();
}

/*
 * CAN ID: 0x1D0
 * Purpose: Engine temperature (displayed on LCD)
 * Frequency: 100ms
 * 
 * Byte 0: Temperature value (temp_celsius + 48)
 * Byte 2: Counter (increments every 200ms)
 * Other bytes: Fixed values for clutch and fuel pressure indicators
 * 
 * Note: This cluster may not have a traditional temp gauge,
 * but displays temperature on the LCD screen.
 */
void sendEngineTemperature() {
  static uint8_t counter = 240;
  static unsigned long lastCounterUpdate = 0;
  
  // Update counter every 200ms
  if (millis() - lastCounterUpdate > 200) {
    counter++;
    if (counter > 254) counter = 240;  // Counter range: 240-254
    lastCounterUpdate = millis();
  }
  
  // Temperature formula: celsius + 48
  uint8_t tempByte = (uint8_t)(g_engine_temperature + 48);
  
  CAN.beginPacket(0x1D0);
  CAN.write(tempByte);  // Temperature
  CAN.write(0xFF);
  CAN.write(0x63);
  CAN.write(0xCD);
  CAN.write(0x5D);
  CAN.write(0x37);
  CAN.write(0xCD);      // Clutch status
  CAN.write(0xA8);      // Fuel/brake pressure
  CAN.endPacket();
}

/*
 * CAN ID: 0x0D7
 * Purpose: Airbag/Seatbelt counter (keeps warnings off)
 * Frequency: 200ms
 * 
 * Incrementing counter tells cluster these systems are active
 */
void sendAirbagSeatbeltCounter() {
  static uint8_t count = 0x00;
  
  CAN.beginPacket(0x0D7);
  CAN.write(count);
  CAN.write(0xFF);
  CAN.endPacket();
  
  count++;
}

/*
 * CAN ID: 0x19E
 * Purpose: ABS brake counter 2 (keeps ABS warning off)
 * Frequency: 200ms
 * 
 * Complex counter that modifies byte 2 to indicate ABS is functioning
 */
void sendABSBrakeCounter2() {
  static uint8_t absFrame[8] = {0x00, 0xE0, 0xB3, 0xFC, 0xF0, 0x43, 0x00, 0x65};
  
  // Update counter in byte 2
  absFrame[2] = ((((absFrame[2] >> 4) + 3) << 4) & 0xF0) | 0x03;
  
  CAN.beginPacket(0x19E);
  CAN.write(absFrame[0]);
  CAN.write(absFrame[1]);
  CAN.write(absFrame[2]);
  CAN.write(absFrame[3]);
  CAN.write(absFrame[4]);
  CAN.write(absFrame[5]);
  CAN.write(absFrame[6]);
  CAN.write(absFrame[7]);
  CAN.endPacket();
}

/*
 * CAN ID: 0x0C0
 * Purpose: ABS brake counter 1 (keeps ABS warning off)
 * Frequency: 200ms
 * 
 * Counter that cycles from 0xF0 to 0xFF
 */
void sendABSBrakeCounter1() {
  static uint8_t count = 0xF0;
  
  CAN.beginPacket(0x0C0);
  CAN.write(count);
  CAN.write(0xFF);
  CAN.endPacket();
  
  // Increment and wrap at 0xFF back to 0xF0
  count = ((count + 1) | 0xF0);
}

/*
 * CAN ID: 0x394 or 0x581
 * Purpose: Seatbelt warning light
 * Frequency: 200ms
 * 
 * Currently hardcoded to OFF (state = false)
 * CAN ID changes based on state
 */
void seatbeltLight(bool state) {
  uint8_t thirdBit = state ? 0x29 : 0x28;
  uint16_t canID = state ? 0x394 : 0x581;
  
  CAN.beginPacket(canID);
  CAN.write(0x40);
  CAN.write(0x4D);
  CAN.write(0x00);
  CAN.write(thirdBit);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.endPacket();
}

// ===== Data Parsing =====

/*
 * Parse incoming SimHub data
 * 
 * Format: SH;field1;field2;field3;...;\n
 * 
 * Field mapping:
 * 0:  "SH" (header)
 * 1:  ignition (true/false or 1/0)
 * 2:  engine_running (true/false or 1/0)
 * 3:  lights_side (0/1)
 * 4:  lights_dip (0/1)
 * 5:  lights_main (0/1)
 * 6:  lights_front_fog (0/1)
 * 7:  lights_rear_fog (0/1)
 * 8:  lights_indicators (0=off, 1=left, 2=right, 3=hazard)
 * 9:  RPM (float)
 * 10: Speed in km/h (float)
 * 11: Fuel percentage (float)
 * 12: Engine temperature in Celsius (float)
 * 13: Handbrake (0/1)
 * 14: ABS (0/1) - not currently used
 * 15: Airbag (0/1) - not currently used
 * 16: Seatbelt (0/1) - not currently used
 * 17-22: Time data (hour, minute, second, day, month, year)
 */
void parseSimHubData() {
  fieldCount = 0;
  int lastSemi = 0;
  
  // Split string by semicolon
  for (unsigned int i = 0; i < inputString.length(); i++) {
    if (inputString[i] == ';') {
      if (fieldCount < 30) {
        fields[fieldCount] = inputString.substring(lastSemi + 1, i);
      }
      fieldCount++;
      lastSemi = i;
    }
  }
  
  // Parse each field into the appropriate variable
  if (fieldCount > 1) g_ignition = (fields[1] == "true" || fields[1] == "1");
  if (fieldCount > 2) g_engine_running = (fields[2] == "true" || fields[2] == "1");
  if (fieldCount > 3) g_lights_side = (fields[3].toInt() == 1);
  if (fieldCount > 4) g_lights_dip = (fields[4].toInt() == 1);
  if (fieldCount > 5) g_lights_main = (fields[5].toInt() == 1);
  if (fieldCount > 6) g_lights_front_fog = (fields[6].toInt() == 1);
  if (fieldCount > 7) g_lights_rear_fog = (fields[7].toInt() == 1);
  if (fieldCount > 8) g_lights_indicators = fields[8].toInt();
  if (fieldCount > 9) g_rpm = (uint16_t)fields[9].toFloat();
  if (fieldCount > 10) g_speed = (uint16_t)fields[10].toFloat();
  if (fieldCount > 11) g_fuel = (uint16_t)fields[11].toFloat();
  if (fieldCount > 12) g_engine_temperature = (uint16_t)fields[12].toFloat();
  if (fieldCount > 13) g_handbrake = (fields[13].toInt() == 1);
}

// ===== Timing Variables =====
uint32_t timestamp100ms = 0;   // For 100ms interval messages
uint32_t timestamp200ms = 0;   // For 200ms interval messages

// ===== Arduino Setup =====
void setup() {
  // Initialize serial communication with SimHub
  Serial.begin(115200);
  
  // Configure CAN shield pins
  CAN.setPins(CS_PIN, IRQ_PIN);
  
  // Set SPI frequency (8MHz clock on shield)
  CAN.setSPIFrequency(1E6);
  
  // Initialize CAN bus at 100kbit/s (BMW K-CAN speed)
  while (!CAN.begin(100E3)) {
    Serial.println("CAN init fail");
    delay(100);
  }
  
  Serial.println("BMW E87 Cluster - Ready!");
  Serial.println("Waiting for SimHub data...");
}

// ===== Main Loop =====
void loop() {
  // Read incoming serial data from SimHub
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n') {
      // Complete message received
      if (inputString.startsWith("SH;")) {
        parseSimHubData();
      }
      inputString = "";  // Clear buffer for next message
    } else {
      inputString += c;  // Build message
    }
  }
  
  // Send turn indicators (has its own timing)
  sendIndicators();
  
  // Send high-frequency messages (100ms interval)
  if (millis() - timestamp100ms > 99) {
    sendIgnitionKeyOn();
    sendIgnitionStatus();
    sendRPM(g_rpm);
    sendSpeed(g_speed);
    sendLights();
    sendEngineTemperature();
    sendHandbrake();
    sendOilPressure();
    sendEML();
    sendSteering();
    
    timestamp100ms = millis();
  }
  
  // Send low-frequency messages (200ms interval)
  if (millis() - timestamp200ms > 199) {
    sendFuelLevel(g_fuel);
    sendAirbagSeatbeltCounter();
    sendABSBrakeCounter2();
    sendABSBrakeCounter1();
    seatbeltLight(false);
    
    timestamp200ms = millis();
  }
}
