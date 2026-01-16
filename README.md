# BMW E87 Instrument Cluster - SimHub Integration

Transform a BMW E87 instrument cluster into a fully functional display for racing simulators!

![Cluster Photo](link-to-your-photo-if-you-want)

## Features

✅ **Working:**
- RPM gauge with accurate engine speed
- Speedometer (displays km/h on inner scale)
- Fuel gauge with percentage display
- Turn indicators with authentic BMW clicking sound
- Headlights, high beams, and fog lights indicators
- Handbrake warning light
- Ignition control (cluster powers on/off with engine)
- Warning lights (ABS, Oil, Check Engine, Steering) - all functioning
- Engine temperature display (shown on LCD)

## Hardware Requirements

- **Arduino Uno** (or compatible board)
- **CAN-BUS Shield V2** (Seeed Studio recommended)
- **BMW E87 Instrument Cluster** (2006 model, part number T2036)
  - Other E87 clusters may work with minor adjustments
- **12V 5A Power Supply** for the cluster
- Jumper wires and basic soldering equipment

## Wiring

### CAN Shield to Arduino
- CS (Chip Select): Pin 9
- INT (Interrupt): Pin 2

### Cluster Connections
- **Pin 1**: CAN-H (connect to shield CAN-H)
- **Pin 2**: CAN-L (connect to shield CAN-L)
- **Pin 3**: Ground (connect to common ground)
- **Pin 4**: +12V (from power supply)

### Important: Termination Resistor
**You MUST cut the termination resistor trace (P1) on the CAN-BUS Shield!** This is critical for proper communication with the cluster. See the [Seeed Wiki](https://wiki.seeedstudio.com/CAN-BUS_Shield_V2.0/) for details.

## Software Setup

### 1. Arduino IDE
Install the required library:
- **CAN by Sandeep Mistry** (install via Library Manager)
- ⚠️ NOT the MCP_CAN library - this project uses a different library!

### 2. Upload Code
- Open `bmw_e87_cluster.ino` in Arduino IDE
- Select your board (Arduino Uno)
- Upload to Arduino

### 3. SimHub Configuration
1. Open SimHub and go to **Settings → Custom Serial Devices**
2. Click **"Add New Device"**
3. Configure:
   - Port: Select your Arduino's COM port
   - Baud rate: **115200**
   - Update message format: **JavaScript**
4. Paste the JavaScript code (see `simhub_script.js` or comments in Arduino code)

## Supported Simulators

Tested and working with:
- Euro Truck Simulator 2 (ETS2)
- American Truck Simulator (ATS)

Should work with any simulator supported by SimHub.

## Troubleshooting

### Cluster doesn't power on
- Check 12V power supply connections
- Verify ground is connected properly
- Ensure ignition message is being sent

### No CAN communication
- **Did you cut the termination resistor trace (P1)?** This is the #1 issue!
- Check CAN-H and CAN-L connections
- Verify CAN shield is properly seated on Arduino
- Check Serial Monitor for "CAN init fail" messages

### Speed reading incorrect
- The speed calculation is calibrated for this specific cluster
- If your cluster shows different speeds, adjust the divisor in `sendSpeed()` function
- Current formula: `speed_kmh / 3` (divide by 3)

### Warning lights always on
- ABS/Oil/EML lights should turn off after a few seconds
- If they stay on, check that counter messages are being sent at 200ms intervals

## Technical Details

### CAN Bus Protocol
- Speed: 100 kbit/s (BMW K-CAN standard)
- Messages are sent at different intervals (100ms or 200ms)
- Based on reverse-engineered protocol from [loopybunny.co.uk](https://www.loopybunny.co.uk/CarPC/k_can.html)

### Key CAN Messages
| ID  | Description | Frequency |
|-----|-------------|-----------|
| 0x130 | Ignition control | 100ms |
| 0x0AA | RPM | 100ms |
| 0x1A6 | Speed/Odometer | 100ms |
| 0x349 | Fuel level | 200ms |
| 0x1F6 | Turn indicators | Variable |
| 0x21A | Lights | 100ms |
| 0x34F | Handbrake | 100ms |

See source code comments for detailed breakdown of each message.

## Credits

- Original inspiration: [TeksuSiK/e87-cluster-simhub](https://github.com/TeksuSiK/e87-cluster-simhub)
- BMW K-CAN protocol: [loopybunny.co.uk](https://www.loopybunny.co.uk/CarPC/k_can.html)
- Community debugging and testing

## License

MIT License - Feel free to use and modify!

## Contributing

Found a bug or have an improvement? Pull requests are welcome!

If you have a different BMW cluster model working, please share your findings so we can support more clusters.

## Disclaimer

This project is for educational and entertainment purposes only. Use at your own risk. The author is not responsible for any damage to your cluster, Arduino, or other equipment.
