# TCRT5000 IR Sensor and ADXL345 LoRa Node

Firmware for an STM32WLE5JC node that combines a TCRT5000 reflective infrared sensor with an LM393 comparator and an ADXL345 accelerometer. It monitors a door state, detects vibration while the door is closed, and reports sensor state, acceleration and battery level over LoRa point-to-point. The node uses RTC wake-ups and low-power shutdown mode.

## Hardware

- **MCU:** STM32WLE5JCIx
- **IR sensor:** TCRT5000 reflective sensor with LM393 comparator; switched power on PB3 and digital output read on PA9
- **Accelerometer:** ADXL345 over I2C2; INT1 on PA0 can wake the node
- **Battery measurement:** internal VREFINT channel sampled by the ADC
- **Radio:** STM32 Sub-GHz radio, LoRa point-to-point at 868.3 MHz and 2 dBm

The IR sensor is powered for a measurement, allowed to settle for 10 ms, sampled five times with 5 ms between samples, then powered off. The majority result is used. The ADXL345 activity detector is configured on X, Y and Z with AC coupling, threshold `0x0F` (about 0.94 g), and low-power 12.5 Hz output data rate.

## Runtime behavior

1. On startup or wake-up, the application restores saved RTC state, including the last IR state, message counter, vibration state and remaining polls before the next periodic report.
2. The IR input is checked every 10 seconds. A periodic report is scheduled every 90 polls (15 minutes). Accelerometer activity and IR state changes can also create pending events.
3. Accelerometer activity is reported as vibration while the IR sensor indicates the door is closed. Event flags are retained in RTC backup registers until the corresponding event is successfully transmitted.
4. Before transmission, the application estimates packet airtime, checks duty-cycle credit, samples the IR sensor and ADXL345, and measures the battery.
5. If battery measurement fails, supply is below 2.4 V, PVD reports low supply, or duty-cycle credit is insufficient, the node skips that transmission attempt.
6. After a successful transmission, the transmitted event flags and report counter are updated. The IR sensor is powered off before the node returns to shutdown.

## LoRa packet

`LoRaNodeData` in `Core/Inc/lora_frame.h` is a packed 15-byte structure transmitted as raw bytes.

| Field | Type | Meaning |
| --- | --- | --- |
| `node_id` | `uint32_t` | Node identifier; currently 3333 |
| `node_type` | `uint8_t` | 3 for the IR-sensor and accelerometer node |
| `msg_counter` | `uint16_t` | Incrementing message counter saved in an RTC backup register |
| `acc_x`, `acc_y`, `acc_z` | `int16_t` each | Raw ADXL345 axis readings |
| `sensor_state` | `uint8_t` | 0 = closed, 1 = open, 2 = vibration |
| `battery_lvl` | `uint8_t` | Estimated battery level from 0 to 100% |

The receiver must use the same field order, sizes and integer interpretation.

## Duty-cycle control

`SubGHz_Phy/App/duty_cycle.c` estimates LoRa packet airtime and keeps an airtime-credit balance and time marker in RTC backup registers. Credit renews over time up to 6 seconds. If the packet cannot be sent within the available balance, the node waits for a later event or report opportunity.

The radio parameters are in `SubGHz_Phy/App/subghz_phy_app.h`. The IR poll interval, periodic report count and node ID are in `SubGHz_Phy/App/subghz_phy_app.c`.

## Project layout

- `Core/Src/main.c` — startup, RTC-state validation and peripheral initialization
- `Core/Src/adxl345.c` — accelerometer configuration and data reads
- `Core/Src/ir_sensor.c` — powered IR-sensor sampling and stable-state reading
- `Core/Src/power_monitor.c` — PVD setup and low-supply status
- `SubGHz_Phy/App/subghz_phy_app.c` — polling, event handling, radio transmission and shutdown scheduling
- `SubGHz_Phy/App/duty_cycle.c` — airtime estimation and duty-cycle accounting
- `ir_accel_node.ioc` — STM32CubeMX peripheral and pin configuration

## Build and flash

1. Open STM32CubeIDE and choose **File → Import → Existing Projects into Workspace**.
2. Select this `ir_accel_node` directory. It contains the `ir_accel_node.ioc` configuration.
3. Build the project, connect an ST-Link debugger/programmer, then use **Run** or **Debug** to flash the target.
