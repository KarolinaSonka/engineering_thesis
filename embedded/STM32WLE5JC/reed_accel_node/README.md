# MC-38 Reed Switch and ADXL345 LoRa Node

Firmware for an STM32WLE5JC node that combines an MC-38 normally-open magnetic reed switch with an ADXL345 accelerometer. It reports reed-switch state changes and vibration events over LoRa point-to-point, along with acceleration and battery information. Between reports, it uses RTC wake-ups and low-power shutdown mode.

## Hardware

- **MCU:** STM32WLE5JCIx
- **Reed switch:** MC-38 normally-open magnetic contact, connected to GPIOB pin 3 (PB3)
- **Accelerometer:** ADXL345 over I2C2; INT1 on PA0 is used as a wake-up source
- **Battery measurement:** internal VREFINT channel sampled by the ADC
- **Radio:** STM32 Sub-GHz radio, LoRa point-to-point at 868.3 MHz and 2 dBm

The ADXL345 activity detector is enabled on X, Y and Z with AC coupling. Its activity threshold is `0x0F` (about 0.94 g), with low-power 12.5 Hz output data rate. Reed state and pending event flags are saved in RTC backup registers so they can be retained across shutdown and retried after a failed transmission.

## Runtime behavior

1. On startup or wake-up, the application checks RTC backup state, the RTC wake-up flag, the reed input and the ADXL345 interrupt to determine why it woke.
2. A reed-state change or accelerometer activity is recorded as a pending event. The node also sends periodic reports every 900 seconds.
3. The application estimates packet airtime and checks the duty-cycle balance, then reads the battery, accelerometer and reed state.
4. If battery measurement fails, supply is below 2.4 V, PVD reports low supply, or duty-cycle credit is insufficient, transmission is skipped and another wake-up is scheduled.
5. After successful transmission, the transmitted event flags and reed state are updated. Vibration reports use a 10-second cooldown. A transmission timeout is charged against the duty-cycle balance before retrying.
6. The node enters shutdown. RTC wake-up, reed input and ADXL345 INT1 provide the configured wake-up paths.

## LoRa packet

`LoRaNodeData` in `Core/Inc/lora_frame.h` is a packed 15-byte structure transmitted as raw bytes.

| Field | Type | Meaning |
| --- | --- | --- |
| `node_id` | `uint32_t` | Node identifier; currently 1111 |
| `node_type` | `uint8_t` | 1 for the reed-switch and accelerometer node |
| `msg_counter` | `uint16_t` | Incrementing message counter saved in an RTC backup register |
| `acc_x`, `acc_y`, `acc_z` | `int16_t` each | Raw ADXL345 axis readings |
| `sensor_state` | `uint8_t` | Project-specific reed state/event encoding; `1` is an open event and `2` is a vibration event |
| `battery_lvl` | `uint8_t` | Estimated battery level from 0 to 100% |

The receiver must use the same field order, sizes and integer interpretation. The `sensor_state` encoding is defined in `SubGHz_Phy/App/subghz_phy_app.c` and `Core/Src/reed_sensor.c`.

## Duty-cycle control

`SubGHz_Phy/App/duty_cycle.c` estimates LoRa packet airtime and keeps an airtime-credit balance and time marker in RTC backup registers. Credit gets bigger over time up to 6 seconds of airtime. If the packet cannot be sent within the available balance, the node sleeps and retries later.

The radio parameters are in `SubGHz_Phy/App/subghz_phy_app.h`. The node ID, report interval and cooldown are in `SubGHz_Phy/App/subghz_phy_app.c`.

## Project layout

- `Core/Src/main.c` — startup, wake-reason detection and peripheral initialization
- `Core/Src/adxl345.c` — accelerometer configuration and data reads
- `Core/Src/reed_sensor.c` — reed-state reading and backup-state handling
- `Core/Src/power_monitor.c` — PVD setup and low-supply status
- `SubGHz_Phy/App/subghz_phy_app.c` — event handling, packet preparation, radio transmission and shutdown scheduling
- `SubGHz_Phy/App/duty_cycle.c` — airtime estimation and duty-cycle accounting
- `reed_accel_node.ioc` — STM32CubeMX peripheral and pin configuration

## Build and flash

1. Open STM32CubeIDE and choose **File → Import → Existing Projects into Workspace**.
2. Select this `reed_accel_node` directory. It contains the `reed_accel_node.ioc` configuration.
3. Build the project, connect an ST-Link debugger/programmer, then use **Run** or **Debug** to flash the target.
