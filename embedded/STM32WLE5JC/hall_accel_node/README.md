# A3114 Hall Sensor and ADXL345 LoRa Node

Firmware for an STM32WLE5JC node that combines an A3114 Hall-effect sensor with an ADXL345 accelerometer. It detects door-state changes and vibration, then reports them over LoRa point-to-point along with acceleration and battery information. The node uses RTC wake-ups and low-power shutdown mode between checks.

## Hardware

- **MCU:** STM32WLE5JCIx
- **Hall sensor:** A3114; powered from PB3 while sampling and its digital output is read on PA9
- **Accelerometer:** ADXL345 over I2C2; INT1 on PA0 can wake the node
- **Battery measurement:** internal VREFINT channel sampled by the ADC
- **Radio:** STM32 Sub-GHz radio, LoRa point-to-point at 868.3 MHz and 2 dBm

The Hall sensor is switched on for each measurement, allowed to settle for 10 ms, then sampled five times with 5 ms between samples. The majority result is used, and the sensor is powered off again. The ADXL345 activity detector is configured on X, Y and Z with AC coupling, threshold `0x0F` (about 0.94 g), and low-power 12.5 Hz output data rate.

## Runtime behavior

1. The application restores its saved RTC state and starts the radio, Hall-poll and vibration timers.
2. The Hall input is sampled every 10 seconds. RTC scheduling also initiates a periodic report every 900 seconds. ADXL345 activity can trigger a wake-up between polls.
3. A Hall-state change is recorded as an event. Accelerometer activity while the Hall sensor indicates the door is closed is treated as vibration. Pending event flags are saved in RTC backup registers until successfully transmitted.
4. Before transmitting, the application estimates packet airtime, checks duty-cycle credit, reads the Hall state and acceleration, and measures the battery.
5. If battery measurement fails, supply is below 2.4 V, PVD reports low supply, or duty-cycle credit is insufficient, the packet is not sent on that pass.
6. After a successful transmission, the completed event flags are cleared and state is saved. The sensor is powered off before the node returns to shutdown.

## LoRa packet

`LoRaNodeData` in `Core/Inc/lora_frame.h` is a packed 15-byte structure transmitted as raw bytes.

| Field | Type | Meaning |
| --- | --- | --- |
| `node_id` | `uint32_t` | Node identifier; currently 2222 |
| `node_type` | `uint8_t` | 2 for the Hall-sensor and accelerometer node |
| `msg_counter` | `uint16_t` | Incrementing message counter saved in an RTC backup register |
| `acc_x`, `acc_y`, `acc_z` | `int16_t` each | Raw ADXL345 axis readings |
| `sensor_state` | `uint8_t` | 0 = closed, 1 = open, 2 = vibration |
| `battery_lvl` | `uint8_t` | Estimated battery level from 0 to 100% |

The receiver must use the same field order, sizes and integer interpretation.

## Duty-cycle control

`SubGHz_Phy/App/duty_cycle.c` estimates LoRa packet airtime and keeps an airtime-credit balance and time marker in RTC backup registers. Credit is replenished over time and capped at 6 seconds of airtime. If the packet cannot be sent within the available balance, the node waits for a later event or report opportunity.

The radio parameters are in `SubGHz_Phy/App/subghz_phy_app.h`. The Hall poll interval, report interval and node ID are in `SubGHz_Phy/App/subghz_phy_app.c`.

## Project layout

- `Core/Src/main.c` — startup, RTC state restoration and peripheral initialization
- `Core/Src/adxl345.c` — accelerometer configuration and data reads
- `Core/Src/hall_sensor.c` — powered Hall-sensor sampling and stable-state reading
- `Core/Src/power_monitor.c` — PVD setup and low-supply status
- `SubGHz_Phy/App/subghz_phy_app.c` — timers, event handling, radio transmission and shutdown scheduling
- `SubGHz_Phy/App/duty_cycle.c` — airtime estimation and duty-cycle accounting
- `hall_accel_node.ioc` — STM32CubeMX peripheral and pin configuration

## Build and flash

1. Open STM32CubeIDE and choose **File → Import → Existing Projects into Workspace**.
2. Select this `hall_accel_node` directory. It contains the `hall_accel_node.ioc` configuration.
3. Build the project, connect an ST-Link debugger/programmer, then use **Run** or **Debug** to flash the target.
