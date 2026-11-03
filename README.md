# South Dakota School of Mines Formula SAE Dashboard

The dashboard used on the 2027 Formula SAE car to communicate the vitals of the car to both the driver and pit personnel.

The dash reads CAN bus data from the car's ECU, displays it on an onboard touchscreen for the driver, and broadcasts it over WiFi so pit crew can watch live data from a laptop.

## Features

- Driver display on a 3.5" touch LCD
- CAN bus data intercepted from the ECU via an MCP2515 CAN-to-SPI board
- Built-in 2.4 GHz WiFi access point serving a live web dashboard
- Live data pushed to connected clients over WebSockets
- External antenna support (IPEX4 output on the final enclosure)

## Hardware

| Component | Part |
|---|---|
| Microcontroller / display | Waveshare ESP32-S3-Touch-LCD-3.5 |
| CAN interface | MCP2515 CAN-to-SPI board |
| Antenna (final design) | External antenna via IPEX4 female connector |

### MCP2515 Wiring

| Signal | ESP32-S3 GPIO |
|---|---|
| CS | 44 |
| MISO | 43 |
| MOSI | 48 |
| SCK | 47 |
| INT | 46 |

### Power and Connections

Currently the board is powered from 5 V USB. The final enclosed device will have the following external connections:

**Inputs**

- Power (5 V to 24 V, ideally)
- Ground
- CAN High
- CAN Low

**Outputs**

- IPEX4 female antenna output

The final design will be housed in a waterproof, vibration-resistant enclosure with a visor.

## Software

Built with [PlatformIO](https://platformio.org/) using the Arduino framework.

### Dependencies

- [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) 1.3.7
- [TCA9554](https://github.com/RobTillaart/TCA9554)
- [mcp_can](https://github.com/coryjfowler/MCP_CAN_lib)
- [WebSockets](https://github.com/Links2004/arduinoWebSockets) ^2.4.1

### `platformio.ini`

```ini
[env:dash]
; espressif32 6.9.0 = Arduino-ESP32 2.0.17
platform = espressif32@6.9.0
board = esp32-s3-devkitc-1
framework = arduino

board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1

monitor_speed = 115200
upload_speed = 921600

lib_deps =
    moononournation/GFX Library for Arduino @ 1.3.7
    robtillaart/TCA9554
    coryjfowler/mcp_can
    links2004/WebSockets @ ^2.4.1
```

### Build and Flash

1. Install [PlatformIO](https://platformio.org/install) (VS Code extension or CLI).
2. Clone this repository and open it in PlatformIO.
3. Connect the board over USB-C.
4. Build and upload:

   ```bash
   pio run -e dash -t upload
   ```

5. Open the serial monitor (115200 baud):

   ```bash
   pio device monitor
   ```

## Usage

1. Power the dash.
2. On a laptop or phone, connect to the WiFi network named **kart dash**.
3. Open [http://192.168.4.1/](http://192.168.4.1/) in a browser to see the live CAN data dashboard.

## Roadmap

- [ ] Feasibility of an LED RPM gauge using the same colors as the LCD gauge
- [ ] Feasibility of super bright LED warning indicators for catastrophic warnings
- [ ] Feasibility of super bright LED shift lights
- [ ] Order IPEX1 to IPEX4 female connector
- [ ] Lay out the final board design
- [ ] Feasibility of IMU data
- [ ] Waterproof, vibration-resistant enclosure with visor

## Team

South Dakota School of Mines Formula SAE.

## License

TODO: add license.
