| Field | Name | Type | Description |
|-------|------|------|-------------|
| 0 | Header | String | Always "SH" |
| 1 | Ignition | Boolean | Key in ignition |
| 2 | Engine Running | Boolean | Engine started |
| 3 | Side Lights | 0/1 | Parking lights |
| 4 | Dip Lights | 0/1 | Low beam |
| 5 | Main Lights | 0/1 | High beam |
| 6 | Front Fog | 0/1 | Front fog lights |
| 7 | Rear Fog | 0/1 | Rear fog lights |
| 8 | Indicators | 0-3 | 0=off, 1=left, 2=right, 3=hazard |
| 9 | RPM | Float | Engine RPM |
| 10 | Speed | Float | Speed in km/h |
| 11 | Fuel | Float | Fuel percentage (0-100) |
| 12 | Temperature | Float | Engine temp in °C |
| 13 | Handbrake | 0/1 | Parking brake engaged |
| 14 | ABS | 0/1 | (Not used) |
| 15 | Airbag | 0/1 | (Not used) |
| 16 | Seatbelt | 0/1 | (Not used) |
| 17 | Hour | 0-23 | Current hour |
| 18 | Minute | 0-59 | Current minute |
| 19 | Second | 0-59 | Current second |
| 20 | Day | 0-6 | Day of week |
| 21 | Month | 0-11 | Month |
| 22 | Year | Number | Full year |

## Custom Properties

To add support for other simulators or additional features, you can modify the JavaScript to use different SimHub properties.

Common properties:
- `$prop('SpeedMph')` - Speed in MPH
- `$prop('Gear')` - Current gear
- `$prop('MaxRpm')` - Maximum RPM
- `$prop('OilPressure')` - Oil pressure
- `$prop('OilTemp')` - Oil temperature

See SimHub documentation for complete property list.
