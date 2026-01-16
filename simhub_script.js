/*
 * BMW E87 Cluster - SimHub JavaScript
 * 
 * This script extracts game data from Euro Truck Simulator 2 / American Truck Simulator
 * and formats it for transmission to the Arduino.
 * 
 * Installation:
 * 1. Open SimHub
 * 2. Go to Settings → Custom Serial Devices
 * 3. Add New Device
 * 4. Set Baud Rate to 115200
 * 5. Set Update Message Format to "JavaScript"
 * 6. Paste this entire script into the text box
 * 
 * Output Format:
 * SH;ignition;engine;side;dip;main;front_fog;rear_fog;indicators;RPM;SPEED;FUEL;temp;handbrake;abs;airbag;seatbelt;hour;minute;second;day;month;year;\n
 */

// === IGNITION AND ENGINE ===
var ignition = $prop('EngineIgnitionOn');
var engine_running = $prop('EngineStarted');

// === LIGHTS ===
// Parking/side lights
var lights_side = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Parking'));

// Low beam headlights
var lights_dip = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamLow'));

// High beam headlights
var lights_main = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamHigh'));

// Front fog lights (using Beacon as ETS2/ATS don't have dedicated fog light property)
var lights_front_fog = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Beacon'));

// Rear fog lights (not available in ETS2/ATS)
var lights_rear_fog = 0;

// === TURN INDICATORS ===
// Get left and right indicator states
var lights_indicator_left_prop = $prop('GameRawData.TruckValues.CurrentValues.LightsValues.BlinkerLeftOn');
var lights_indicator_right_prop = $prop('GameRawData.TruckValues.CurrentValues.LightsValues.BlinkerRightOn');

// Determine indicator state:
// 0 = off, 1 = left, 2 = right, 3 = hazard (both)
var lights_indicators;
if (lights_indicator_left_prop && lights_indicator_right_prop) {
    lights_indicators = 3; // Hazard lights (both on)
} else if (lights_indicator_left_prop) {
    lights_indicators = 1; // Left indicator
} else if (lights_indicator_right_prop) {
    lights_indicators = 2; // Right indicator
} else {
    lights_indicators = 0; // Indicators off
}

// === ENGINE DATA ===
// Engine RPM
var rpm = $prop('Rpms');

// Vehicle speed in km/h
var speed = $prop('SpeedKmh');

// === FUEL ===
// Calculate fuel percentage from current and max fuel
var currentFuel = $prop('Fuel');
var maxFuel = $prop('MaxFuel');

function calculate(current, max) {
    return (current / max) * 100;
}

var fuel = calculate(currentFuel, maxFuel);

// Handle case where fuel data is not available
if (isNaN(fuel)) {
    fuel = 0;
}

// === ENGINE TEMPERATURE ===
// Water/coolant temperature in Celsius
var engine_temperature = $prop('GameRawData.TruckValues.CurrentValues.DashboardValues.WaterTemperature');

// === WARNING LIGHTS ===
// Handbrake/parking brake status
var handbrake = Number($prop('GameRawData.TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake'));

// ABS warning (not used in ETS2/ATS - always off)
var abs = 0;

// Airbag warning (not used - always off)
var airbag = 0;

// Seatbelt warning (not used - always off)
var seatbelt = 0;

// === TIME DATA ===
// Current system time (for potential clock display on cluster LCD)
var currentTime = new Date();
var hour = currentTime.getHours();
var minute = currentTime.getMinutes();
var second = currentTime.getSeconds();
var year = currentTime.getFullYear();
var month = currentTime.getMonth();
var day = currentTime.getDay();

// === BUILD OUTPUT MESSAGE ===
// Format: SH;field1;field2;field3;...;\n
// Fields are separated by semicolons, message ends with newline
return `SH;${ignition};${engine_running};${lights_side};${lights_dip};${lights_main};${lights_front_fog};${lights_rear_fog};${lights_indicators};${rpm};${speed};${fuel};${engine_temperature};${handbrake};${abs};${airbag};${seatbelt};${hour};${minute};${second};${day};${month};${year};\n`;
