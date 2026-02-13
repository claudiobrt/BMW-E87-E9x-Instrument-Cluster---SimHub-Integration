/*
 * BMW E87 - Complete Script with Time (Exact Original Format)
 */

// ===== IGNITION AND ENGINE =====
var ignition = $prop('EngineIgnitionOn') ? 1 : 0;
var engine_running = $prop('EngineStarted') ? 1 : 0;

// ===== OIL TEMPERATURE (Field 3) =====
var oil_temperature = $prop('OilTemperature');
if (!oil_temperature || isNaN(oil_temperature)) {
    oil_temperature = $prop('GameRawData.BeamNG.electrics.oiltemp');
}
if (!oil_temperature || isNaN(oil_temperature)) {
    oil_temperature = $prop('GameRawData.TruckValues.CurrentValues.DashboardValues.OilTemperature');
}
if (!oil_temperature || isNaN(oil_temperature)) {
    oil_temperature = $prop('OilTemp');
}
if (!oil_temperature || isNaN(oil_temperature)) {
    var water = $prop('WaterTemperature');
    oil_temperature = water || 0;
}
// CRITICAL: Round to integer to prevent decimals from corrupting message parsing
oil_temperature = Math.round(oil_temperature);

// ===== LIGHTS - Universal approach =====
var lights_side_beamng = Number($prop('GameRawData.BeamNG.lightsState.lightbar'));
var lights_dip_beamng = Number($prop('GameRawData.BeamNG.lightsState.lowBeam'));
var lights_main_beamng = Number($prop('GameRawData.BeamNG.lightsState.highBeam'));
var lights_front_fog_beamng = Number($prop('GameRawData.BeamNG.lightsState.fogLights'));

var lights_side_truck = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Parking'));
var lights_dip_truck = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamLow'));
var lights_main_truck = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.BeamHigh'));
var lights_front_fog_truck = Number($prop('GameRawData.TruckValues.CurrentValues.LightsValues.Beacon'));

var lights_side = (lights_side_beamng || lights_side_truck) ? 1 : 0;
var lights_dip = (lights_dip_beamng || lights_dip_truck) ? 1 : 0;
var lights_main = (lights_main_beamng || lights_main_truck) ? 1 : 0;
var lights_front_fog = (lights_front_fog_beamng || lights_front_fog_truck) ? 1 : 0;
var lights_rear_fog = 0;

// ===== TURN INDICATORS =====
var indicator_left = $prop('DataCorePlugin.GameData.NewData.TurnIndicatorLeft');
var indicator_right = $prop('DataCorePlugin.GameData.NewData.TurnIndicatorRight');

var lights_indicators = 0;
if (indicator_left && indicator_right) {
    lights_indicators = 3;
} else if (indicator_left) {
    lights_indicators = 1;
} else if (indicator_right) {
    lights_indicators = 2;
}

// ===== ENGINE DATA =====
var rpm = $prop('Rpms');
if (!rpm || isNaN(rpm)) rpm = 0;

var speed = $prop('SpeedKmh');
if (!speed || isNaN(speed)) speed = 0;

// ===== FUEL =====
var currentFuel = $prop('Fuel');
var maxFuel = $prop('MaxFuel');
var fuel = 0;

if (maxFuel && maxFuel > 0) {
    fuel = (currentFuel / maxFuel) * 100;
}
if (isNaN(fuel) || fuel < 0) fuel = 0;
if (fuel > 100) fuel = 100;

// ===== WATER TEMPERATURE (Field 12) =====
var engine_temperature = $prop('WaterTemperature');
if (!engine_temperature || isNaN(engine_temperature)) {
    engine_temperature = $prop('GameRawData.TruckValues.CurrentValues.DashboardValues.WaterTemperature');
}
if (!engine_temperature || isNaN(engine_temperature)) engine_temperature = 0;

// ===== HANDBRAKE =====
var handbrake_beamng = $prop('Handbrake');
var handbrake_truck = $prop('GameRawData.TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake');
var handbrake = (handbrake_beamng || handbrake_truck) ? 1 : 0;

// ===== GEAR CALCULATION =====
var current_gear = 1;
var gear_mode_char = "N";

var game_gear = $prop('Gear');
var is_reverse = $prop('GameRawData.TruckValues.CurrentValues.DashboardValues.GearDashboards.reverse');

if (game_gear === 'R' || game_gear === -1 || is_reverse) {
    current_gear = 0;
    gear_mode_char = "R";
} else if (game_gear === 'N' || game_gear === 0 || game_gear === null || game_gear === undefined) {
    current_gear = 1;
    gear_mode_char = "N";
} else if (game_gear === 'P') {
    current_gear = 1;
    gear_mode_char = "P";
} else {
    var gear_number = parseInt(game_gear);
    if (!isNaN(gear_number) && gear_number >= 1) {
        current_gear = Math.min(gear_number + 1, 8);
        gear_mode_char = "M";
    } else {
        current_gear = 1;
        gear_mode_char = "N";
    }
}

// ===== BUILD OUTPUT =====
// Use exact format from original custom_protocol.txt
// Round ALL numeric values to prevent decimals from corrupting field positions
return 'SH;' + ignition + ';' + engine_running + ';' + Math.round(oil_temperature) + ';' +
       lights_side + ';' + lights_dip + ';' + lights_main + ';' + 
       lights_front_fog + ';' + lights_rear_fog + ';' + lights_indicators + ';' + 
       Math.round(rpm) + ';' + Math.round(speed) + ';' + Math.round(fuel) + ';' + 
       Math.round(engine_temperature) + ';' + 
       handbrake + ';' + current_gear + ';' + gear_mode_char + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'yyyy') + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'MM') + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'dd') + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'HH') + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'mm') + ';' +
       format($prop('DataCorePlugin.CurrentDateTime'), 'ss') + '\n';
