/*
 * BMW E87 - FINAL WORKING VERSION WITH GEAR + HANDBRAKE FILTERING
 * 
 * Features:
 * ✅ RPM, Speed, Fuel, Gears (M1-M7), Indicators
 * ✅ Handbrake with glitch filtering
 * ✅ Gear with glitch filtering (ignores brief N/R changes)
 */

#include <CAN.h>

#define lo8(x) (uint8_t)((x)&0xff)
#define hi8(x) (uint8_t)(((x) >> 8) & 0xff)

const uint8_t CS_PIN = 9;
const uint8_t IRQ_PIN = 2;

String inputString = "";
String fields[30];
int fieldCount = 0;

uint16_t g_rpm = 800;
uint16_t g_speed = 0;
uint16_t g_fuel = 50;
uint8_t g_lights_indicators = 0;
uint16_t g_engine_temperature = 90;
uint16_t g_oil_temperature = 90;  // Oil temperature
uint8_t g_current_gear = 1;
char g_gear_mode = 'N';
bool g_handbrake = false;
bool g_ignition = false;
bool g_engine_running = false;
bool g_lights_side = false;
bool g_lights_dip = false;
bool g_lights_main = false;
bool g_lights_front_fog = false;
bool g_lights_rear_fog = false;

// Time and date
uint16_t g_time_year = 2024;
uint8_t g_time_month = 1;
uint8_t g_time_day = 1;
uint8_t g_time_hour = 12;
uint8_t g_time_minute = 0;
uint8_t g_time_second = 0;

// ===== GLITCH FILTER SETTINGS =====
// Adjust these values to tune the filter delay vs stability tradeoff:
// - Lower values = faster response but may allow some glitches through
// - Higher values = more stable but slower to respond to real changes
const uint32_t HANDBRAKE_FILTER_MS = 1000; // Handbrake must be stable for this long (increased to 1s)
const uint32_t GEAR_FILTER_MS = 200;       // Gear must be stable for this long
const uint32_t OIL_TEMP_FILTER_MS = 1000;  // Oil temp must be stable for this long

// Filtering for handbrake
bool g_handbrake_raw = false;
uint32_t g_handbrake_change_time = 0;

// Filtering for gear
uint8_t g_current_gear_raw = 1;
char g_gear_mode_raw = 'N';
uint32_t g_gear_change_time = 0;

// Filtering for oil temperature
uint16_t g_oil_temperature_raw = 90;
uint32_t g_oil_temp_change_time = 0;

#define SIDE 0x01
#define DIP 0x02
#define MAIN 0x04
#define FRONT_FOG 0x10
#define REAR_FOG 0x20

uint32_t timestamp100ms = 0;
uint32_t timestamp200ms = 0;
uint32_t timestamp50ms = 0;  // New faster loop for steering

void sendIgnitionKeyOn() {
  static uint8_t counter = 0;
  uint8_t byte0 = g_engine_running ? 0x45 : (g_ignition ? 0x41 : 0x00);
  CAN.beginPacket(0x130);
  CAN.write(byte0); CAN.write(0x40); CAN.write(0x21); CAN.write(0x8F); CAN.write(counter++);
  CAN.endPacket();
}

void sendIgnitionStatus() {
  uint8_t byte0, byte1, byte2;
  if (g_engine_running) { byte0 = 0x40; byte1 = 0x40; byte2 = 0x7F; }
  else if (g_ignition) { byte0 = 0x00; byte1 = 0x40; byte2 = 0x7F; }
  else { byte0 = 0x00; byte1 = 0x00; byte2 = 0x3F; }
  CAN.beginPacket(0x26E);
  CAN.write(byte0); CAN.write(byte1); CAN.write(byte2); CAN.write(0x50);
  CAN.write(0xFF); CAN.write(0xFF); CAN.write(0xFF); CAN.write(0xFF);
  CAN.endPacket();
}

void sendRPM(uint16_t rpm) {
  uint16_t tempRpm = rpm * 4;
  CAN.beginPacket(0x0AA);
  CAN.write(0xFE); CAN.write(0xFE); CAN.write(0xFF); CAN.write(0x00);
  CAN.write(lo8(tempRpm)); CAN.write(hi8(tempRpm)); CAN.write(0xFE); CAN.write(0x99);
  CAN.endPacket();
}

void sendFuelLevel(uint16_t percent) {
  uint16_t sensor;
  if (percent >= 90) sensor = 9000 + ((percent - 90) * 700 / 10);
  else if (percent >= 75) sensor = 7000 + ((percent - 75) * 2000 / 15);
  else if (percent >= 50) sensor = 4500 + ((percent - 50) * 2500 / 25);
  else if (percent >= 25) sensor = 2000 + ((percent - 25) * 2500 / 25);
  else if (percent >= 10) sensor = 500 + ((percent - 10) * 1500 / 15);
  else sensor = percent * 50;
  CAN.beginPacket(0x349);
  CAN.write(lo8(sensor)); CAN.write(hi8(sensor));
  CAN.write(lo8(sensor)); CAN.write(hi8(sensor)); CAN.write(0x00);
  CAN.endPacket();
}

void sendSpeed(uint16_t speed_kmh) {
  static uint32_t lastTimeSent = 0;
  static uint16_t lastReading = 0;
  static uint16_t counter = 0x0010;
  uint32_t now = millis();
  if (lastTimeSent == 0) { lastTimeSent = now; return; }
  uint16_t speed_adjusted = speed_kmh / 3;
  uint32_t timeDiff = now - lastTimeSent;
  uint16_t speedValue = lastReading + ((timeDiff / 50) * speed_adjusted);
  lastReading = speedValue; lastTimeSent = now;
  counter += (timeDiff / 50) * 100;
  uint16_t counterMasked = counter & 0x0FFF;
  CAN.beginPacket(0x1A6);
  CAN.write(lo8(speedValue)); CAN.write(hi8(speedValue));
  CAN.write(lo8(speedValue)); CAN.write(hi8(speedValue));
  CAN.write(lo8(speedValue)); CAN.write(hi8(speedValue));
  CAN.write(lo8(counterMasked)); CAN.write(hi8(counterMasked) | 0xF0);
  CAN.endPacket();
}

void sendIndicators() {
  static uint8_t lastIndicator = 0;
  static uint32_t lastIndicatorTime = 0, lastFrameTime = 0;
  uint32_t current = millis();
  uint8_t lightIndicator = lastIndicator;
  if (g_lights_indicators == 0) {
    if (current - lastIndicatorTime >= 600) lightIndicator = g_lights_indicators;
  } else { lightIndicator = g_lights_indicators; lastIndicatorTime = current; }
  if ((lastIndicator != lightIndicator) || (current - lastFrameTime >= 600)) {
    uint8_t byte0, byte1;
    if (lightIndicator != 0) {
      switch (lightIndicator) {
        case 1: byte0 = 0x91; break; case 2: byte0 = 0xA1; break;
        case 3: byte0 = 0xB1; break; default: byte0 = 0x80; break;
      }
      byte1 = (lastIndicator == lightIndicator) ? 0xF1 : 0xF2;
    } else { byte0 = 0x80; byte1 = 0xF0; }
    lastIndicator = lightIndicator; lastFrameTime = current;
    CAN.beginPacket(0x1F6); CAN.write(byte0); CAN.write(byte1); CAN.endPacket();
  }
}

void sendLights() {
  uint8_t lights = 0x00;
  
  // NOTE: This cluster has swapped bit positions for some lights
  // Auto-enable backlight when ignition is on
  if (g_ignition || g_engine_running) {
    lights |= MAIN; // Backlight always on with ignition
  }
  
  // Other lights only if explicitly turned on in game
  if (g_lights_side) lights |= MAIN;    // Parking/side lights -> MAIN bit
  if (g_lights_dip) lights |= SIDE;     // Low beams -> SIDE bit  
  if (g_lights_main) lights |= DIP;     // High beams -> DIP bit
  if (g_lights_front_fog) lights |= FRONT_FOG;
  if (g_lights_rear_fog) lights |= REAR_FOG;
  
  CAN.beginPacket(0x21A); CAN.write(lights); CAN.write(0x00); CAN.write(0xF7); CAN.endPacket();
}

void sendHandbrake() {
  CAN.beginPacket(0x34F); 
  CAN.write(g_handbrake ? 0xFE : 0xFD); 
  CAN.write(0xFF); 
  CAN.endPacket();
}

void sendOilPressure() {
  CAN.beginPacket(0x332); CAN.write(0x00); CAN.write(0x00); CAN.write(0x00); CAN.endPacket();
}

void sendEML() {
  CAN.beginPacket(0x545); CAN.write(0xFC); CAN.write(0xFF); CAN.endPacket();
}

void sendSteering() {
  // Send steering wheel position (CAN 0x0C4)
  CAN.beginPacket(0x0C4);
  CAN.write(0x00);
  CAN.write(0x80);  // Steering position low
  CAN.write(0xFD);  // Steering position high
  CAN.write(0x00);
  CAN.write(0x80);
  CAN.write(0xFF);
  CAN.write(0xF6);
  CAN.endPacket();
}

void canSendErrorLight(uint16_t light_id, bool enable) {
  // CAN ID 0x592 - Symbol/warning light system
  // Format: 0x40, light_id_low, light_id_high, state, 0xFF, 0xFF, 0xFF, 0xFF
  const uint8_t ON = 0x31;
  const uint8_t OFF = 0x30;
  
  CAN.beginPacket(0x592);
  CAN.write(0x40);
  CAN.write(lo8(light_id));
  CAN.write(hi8(light_id));
  CAN.write(enable ? ON : OFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.write(0xFF);
  CAN.endPacket();
}

void canSendDmeStatus() {
  // CAN ID 0x12F - DME (Engine Management) status
  // This tells the cluster the engine is healthy (no check engine light)
  CAN.beginPacket(0x12F);
  CAN.write(0x3F);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x40);
  CAN.write(0x01);
  CAN.write(0x00);
  CAN.endPacket();
}

void canSuppressSos() {
  // CAN ID 0x0C1 - SOS warning suppression
  // Use random byte in first position
  CAN.beginPacket(0x0C1);
  CAN.write((uint8_t)random(0, 256));
  CAN.write(0xFF);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
}

void sendTime() {
  // CAN ID 0x39E - Time and date display
  CAN.beginPacket(0x39E);
  CAN.write(g_time_hour);
  CAN.write(g_time_minute);
  CAN.write(g_time_second);
  CAN.write(g_time_day);
  CAN.write((uint8_t)((g_time_month << 4) | 0x0F));  // Month in upper 4 bits
  CAN.write((uint8_t)(g_time_year & 0xFF));          // Year low byte
  CAN.write((uint8_t)((g_time_year >> 8) & 0xFF));   // Year high byte
  CAN.write(0xF2);
  CAN.endPacket();
}

void suppressSteeringWarning() {

  canSendErrorLight(73, false);
}
void suppressServiceLight() {

  canSendErrorLight(281, false);
}
void suppressCheckEngineLight() {
  canSendErrorLight(34, false);
}
void suppressCheckEngineDouble() {
  canSendErrorLight(31, false);
}

void sendGearbox() {
  uint8_t byte0 = 0x00;
  if (g_gear_mode == 'P') byte0 = 0xE1;
  else if (g_current_gear == 0) byte0 = 0xD2;
  else if (g_current_gear == 1) byte0 = 0xB4;
  else byte0 = 0x78;
  
  uint8_t byte1 = 0x0F;
  if (g_gear_mode == 'M' || g_gear_mode == 'S') {
    switch (g_current_gear) {
      case 2: byte1 = 0x50; break; case 3: byte1 = 0x60; break;
      case 4: byte1 = 0x70; break; case 5: byte1 = 0x80; break;
      case 6: byte1 = 0x90; break; case 7: byte1 = 0xA0; break;
      case 8: byte1 = 0xB0; break; default: byte1 = 0x00; break;
    }
  }
  
  static uint8_t counter_high = 0;
  uint8_t byte3;
  if (g_gear_mode == 'P' || g_current_gear == 0) {
    byte3 = (counter_high << 4) | 0x0C;
    counter_high = (counter_high + 1) % 16;
  } else {
    byte3 = (counter_high << 4) | (g_gear_mode == 'S' ? 0x07 : 0x0D);
    counter_high = (counter_high + 1) % 15;
  }
  
  uint8_t byte4 = (g_gear_mode == 'M' || g_gear_mode == 'S') ? 0xF2 : 0xF0;
  CAN.beginPacket(0x1D2);
  CAN.write(byte0); CAN.write(byte1); CAN.write(0xFF); CAN.write(byte3);
  CAN.write(byte4); CAN.write(0xFF); CAN.write(0xFF); CAN.write(0xFF);
  CAN.endPacket();
}

void sendEngineTemperature() {
  // CAN ID 0x1D0 sends both water temp (byte 0) and oil temp (byte 1)
  static uint8_t alive_counter = 0;
  static unsigned long lastUpdate = 0;
  
  uint32_t now = millis();
  if (now - lastUpdate > 100) {  // Update every 100ms
    alive_counter = (alive_counter + 1) & 0x0F; // Increment and keep lower 4 bits
    lastUpdate = now;
  }
  
  // Engine run state: 0x0 = off, 0x1 = starting, 0x2 = running
  uint8_t engine_run_state = g_engine_running ? 0x2 : 0x0;
  
  // Byte 2: Combine alive counter (lower 4 bits) and engine state (bits 4-5)
  uint8_t byte2 = alive_counter | ((engine_run_state & 0x03) << 4);
  
  CAN.beginPacket(0x1D0);
  CAN.write((uint8_t)(g_engine_temperature + 48)); // Byte 0: Water temperature
  CAN.write((uint8_t)(g_oil_temperature + 48));    // Byte 1: Oil temperature
  CAN.write(byte2);                                 // Byte 2: Alive counter + engine state
  CAN.write(0xCD); // Byte 3
  CAN.write(0x5D); // Byte 4: Fuel injection low (dummy)
  CAN.write(0x37); // Byte 5: Fuel injection high (dummy)
  CAN.write(0xCD); // Byte 6
  CAN.write(0xA8); // Byte 7
  CAN.endPacket();
}

void sendAirbagSeatbeltCounter() {
  static uint8_t count = 0x00;
  CAN.beginPacket(0x0D7); CAN.write(count++); CAN.write(0xFF); CAN.endPacket();
}

void sendABSBrakeCounter2() {
  static uint8_t absFrame[8] = {0x00, 0xE0, 0xB3, 0xFC, 0xF0, 0x43, 0x00, 0x65};
  absFrame[2] = ((((absFrame[2] >> 4) + 3) << 4) & 0xF0) | 0x03;
  CAN.beginPacket(0x19E);
  for (int i = 0; i < 8; i++) CAN.write(absFrame[i]);
  CAN.endPacket();
}

void sendABSBrakeCounter1() {
  static uint8_t count = 0xF0;
  CAN.beginPacket(0x0C0); CAN.write(count); CAN.write(0xFF); CAN.endPacket();
  count = ((count + 1) | 0xF0);
}

void seatbeltLight(bool state) {
  uint16_t canID = state ? 0x394 : 0x581;
  CAN.beginPacket(canID); CAN.write(0x40); CAN.write(0x4D); CAN.write(0x00);
  CAN.write(state ? 0x29 : 0x28);
  CAN.write(0xFF); CAN.write(0xFF); CAN.write(0xFF); CAN.write(0xFF);
  CAN.endPacket();
}

void updateHandbrakeFiltered() {
  // HANDBRAKE FILTERING: Ignore brief glitches
  static bool last_raw = false;
  uint32_t now = millis();
  
  if (g_handbrake_raw != last_raw) {
    g_handbrake_change_time = now;
    last_raw = g_handbrake_raw;
  }
  
  if ((now - g_handbrake_change_time) > HANDBRAKE_FILTER_MS) {
    g_handbrake = g_handbrake_raw;
  }
}

void updateGearFiltered() {
  // GEAR FILTERING: Ignore brief gear changes (especially random N)
  static uint8_t last_gear_raw = 1;
  static char last_mode_raw = 'N';
  uint32_t now = millis();
  
  // Check if either gear or mode changed
  if (g_current_gear_raw != last_gear_raw || g_gear_mode_raw != last_mode_raw) {
    g_gear_change_time = now;
    last_gear_raw = g_current_gear_raw;
    last_mode_raw = g_gear_mode_raw;
  }
  
  // If stable for long enough, update the filtered values
  if ((now - g_gear_change_time) > GEAR_FILTER_MS) {
    g_current_gear = g_current_gear_raw;
    g_gear_mode = g_gear_mode_raw;
  }
}

void updateOilTempFiltered() {
  // OIL TEMP FILTERING: Smooth out temperature changes
  static uint16_t last_oil_temp_raw = 90;
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  
  // Debug: Print oil temp values every 2 seconds
  if (now - lastPrint > 2000) {
    Serial.print("OIL TEMP: raw=");
    Serial.print(g_oil_temperature_raw);
    Serial.print(" filtered=");
    Serial.println(g_oil_temperature);
    lastPrint = now;
  }
  
  // Only update if temperature changed significantly (more than 2 degrees)
  if (abs((int)g_oil_temperature_raw - (int)last_oil_temp_raw) > 2) {
    g_oil_temp_change_time = now;
    last_oil_temp_raw = g_oil_temperature_raw;
  }
  
  // If stable for long enough, update the filtered value
  if ((now - g_oil_temp_change_time) > OIL_TEMP_FILTER_MS) {
    g_oil_temperature = g_oil_temperature_raw;
  }
}

void parseSimHubData() {
  fieldCount = 0;
  int lastSemi = 0;
  inputString.trim();
  
  for (unsigned int i = 0; i < inputString.length(); i++) {
    if (inputString[i] == ';') {
      if (fieldCount < 30) {
        fields[fieldCount] = inputString.substring(lastSemi + 1, i);
        fields[fieldCount].trim();
      }
      fieldCount++;
      lastSemi = i;
    }
  }
  
  if (lastSemi < (int)inputString.length() - 1 && fieldCount < 30) {
    fields[fieldCount] = inputString.substring(lastSemi + 1);
    fields[fieldCount].trim();
    fieldCount++;
  }
  
  if (fieldCount > 1) g_ignition = (fields[1] == "true" || fields[1] == "1");
  if (fieldCount > 2) g_engine_running = (fields[2] == "true" || fields[2] == "1");
  if (fieldCount > 3) g_oil_temperature_raw = (uint16_t)fields[3].toFloat();  
  if (fieldCount > 4) g_lights_side = (fields[4].toInt() == 1);
  if (fieldCount > 5) g_lights_dip = (fields[5].toInt() == 1);
  if (fieldCount > 6) g_lights_main = (fields[6].toInt() == 1);
  if (fieldCount > 7) g_lights_front_fog = (fields[7].toInt() == 1);
  if (fieldCount > 8) g_lights_rear_fog = (fields[8].toInt() == 1);
  if (fieldCount > 9) g_lights_indicators = fields[9].toInt();
  if (fieldCount > 10) g_rpm = (uint16_t)fields[10].toFloat();
  if (fieldCount > 11) g_speed = (uint16_t)fields[11].toFloat();
  if (fieldCount > 12) g_fuel = (uint16_t)fields[12].toFloat();
  if (fieldCount > 13) g_engine_temperature = (uint16_t)fields[13].toFloat();  
  if (fieldCount > 14) g_handbrake_raw = (fields[14].toInt() == 1);
  if (fieldCount > 15) g_current_gear_raw = (uint8_t)fields[15].toInt();
  if (fieldCount > 16) {
    String mode = fields[16];
    if (mode.length() > 0) g_gear_mode_raw = mode.charAt(0);
  }
  // Time and date (fields 17-22)
  // Debug: Print RAW values BEFORE validation
  static uint32_t lastTimePrint = 0;
  if (millis() - lastTimePrint > 2000) {
    Serial.print("RAW TIME FIELDS: [");
    if (fieldCount > 17) Serial.print(fields[17]); else Serial.print("?");
    Serial.print(";");
    if (fieldCount > 18) Serial.print(fields[18]); else Serial.print("?");
    Serial.print(";");
    if (fieldCount > 19) Serial.print(fields[19]); else Serial.print("?");
    Serial.print(";");
    if (fieldCount > 20) Serial.print(fields[20]); else Serial.print("?");
    Serial.print(";");
    if (fieldCount > 21) Serial.print(fields[21]); else Serial.print("?");
    Serial.print(";");
    if (fieldCount > 22) Serial.print(fields[22]); else Serial.print("?");
    Serial.print("] -> Display: ");
    Serial.print(g_time_hour); Serial.print(":");
    Serial.print(g_time_minute); Serial.print(":");
    Serial.println(g_time_second);
    lastTimePrint = millis();
  }
  
  // Only update time if we receive valid values (not 0:0:0)
  if (fieldCount > 17) {
    uint16_t new_year = (uint16_t)fields[17].toInt();
    if (new_year > 2000) g_time_year = new_year;  // Only update if valid year
  }
  if (fieldCount > 18) {
    uint8_t new_month = (uint8_t)fields[18].toInt();
    if (new_month >= 1 && new_month <= 12) g_time_month = new_month;  // Only if valid month
  }
  if (fieldCount > 19) {
    uint8_t new_day = (uint8_t)fields[19].toInt();
    if (new_day >= 1 && new_day <= 31) g_time_day = new_day;  // Only if valid day
  }
  if (fieldCount > 20) {
    uint8_t new_hour = (uint8_t)fields[20].toInt();
    if (new_hour <= 23) g_time_hour = new_hour;  // Only if valid hour (0-23)
  }
  if (fieldCount > 21) {
    uint8_t new_minute = (uint8_t)fields[21].toInt();
    if (new_minute <= 59) g_time_minute = new_minute;  // Only if valid minute
  }
  if (fieldCount > 22) {
    uint8_t new_second = (uint8_t)fields[22].toInt();
    if (new_second <= 59) g_time_second = new_second;  // Only if valid second
  }
}

void setup() {
  Serial.begin(115200);
  CAN.setPins(CS_PIN, IRQ_PIN);
  CAN.setSPIFrequency(1E6);
  while (!CAN.begin(100E3)) {
    Serial.println("CAN init fail");
    delay(100);
  }
  Serial.println("BMW E87 Cluster - Ready!");
  Serial.println("Features: RPM, Speed, Fuel, Gears, Indicators");
  Serial.println("Glitch filtering: Handbrake (500ms), Gear (200ms)");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputString.length() > 0 && inputString.startsWith("SH")) {
        parseSimHubData();
        updateHandbrakeFiltered();
        updateGearFiltered();
        updateOilTempFiltered(); // Apply oil temp filter
      }
      inputString = "";
    } else {
      inputString += c;
    }
  }
  
  sendIndicators();
  
  // CRITICAL: Suppress steering warning on EVERY loop (no delay)
  // This prevents random icon flickering
  suppressSteeringWarning();
  
  // 50ms loop - Steering position
  if (millis() - timestamp50ms > 49) {
    sendSteering();           // Send steering position
    timestamp50ms = millis();
  }
  
  if (millis() - timestamp100ms > 99) {
    sendIgnitionKeyOn(); sendIgnitionStatus(); sendRPM(g_rpm); sendSpeed(g_speed);
    sendLights(); sendEngineTemperature(); sendGearbox(); sendHandbrake();
    sendTime(); // Send time/date to cluster
    // sendOilPressure(); // DISABLED - preventing warnings
    // sendEML(); // DISABLED - preventing warnings
    timestamp100ms = millis();
  }
  
  if (millis() - timestamp200ms > 199) {
    sendFuelLevel(g_fuel); sendAirbagSeatbeltCounter();
    sendABSBrakeCounter2(); sendABSBrakeCounter1(); seatbeltLight(false);
    // suppressSteeringWarning moved to 50ms loop for more frequent updates
    suppressServiceLight();      // Turn off service light
    suppressCheckEngineLight();  // Turn off check engine light (symbol)
    suppressCheckEngineDouble(); // Turn off double check engine light
    canSendDmeStatus();          // Send DME status OK
    canSuppressSos();            // Suppress SOS warning
    timestamp200ms = millis();
  }
}
