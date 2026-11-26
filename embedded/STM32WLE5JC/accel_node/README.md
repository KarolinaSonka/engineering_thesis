# Accelerometer LoRa Node

Firmware for a standalone STM32WLE5JC node with an ADXL345 accelerometer. The node detects activity, measures acceleration and battery voltage, and sends a compact LoRa point-to-point packet. It uses the MCU's RTC and low-power shutdown mode between reports.

## Hardware

- **MCU:** STM32WLE5JCIx
- **Accelerometer:** ADXL345, connected over I2C2
- **Activity interrupt:** ADXL345 INT1 on PA0, used as a wake-up source
- **Battery measurement:** internal VREFINT channel sampled by the ADC
- **Radio:** STM32 Sub-GHz radio, configured for LoRa point-to-point at 868.3 MHz and 2 dBm

The ADXL345 activity detector is enabled on X, Y and Z with AC coupling. The activity threshold register is set to `0x0F` (about 0.94 g), and the sensor uses low-power 12.5 Hz output data rate.

## Runtime behavior

1. On startup or wake-up, the application checks RTC backup registers and the RTC wake-up flag to classify the cause: boot, periodic wake-up, accelerometer activity, or cooldown completion. An inconsistent saved state is treated as an error.
2. The ADXL345 is initialized and configured for activity detection.
3. The radio task checks the expected packet airtime and duty-cycle credits, reads the battery voltage and the three acceleration axes, and prepares a packet.
4. If the battery measurement fails, the voltage is below 2.4 V, the PVD reports low supply, or duty-cycle credits are insufficient, the node skips transmission and schedules another wake-up.
5. Otherwise, it sends the packet. A successful transmission clears the saved pending-alert flag. A transmission timeout is charged conservatively before the next retry.
6. The node returns to shutdown. Periodic reports are scheduled every 900 seconds; alert reports use a 10-second cooldown. During the periodic sleep stage, ADXL345 INT1 is enabled as a wake-up source for activity.

## LoRa packet

`LoRaNodeData` in `Core/Inc/lora_frame.h` is packed and transmitted as raw bytes. Its current size is 15 bytes.

| Field | Type | Meaning |
| --- | --- | --- |
| `node_id` | `uint32_t` | Node identifier; currently configured as 0 |
| `node_type` | `uint8_t` | 0 for the accelerometer-only node |
| `msg_counter` | `uint16_t` | Incrementing message counter, saved in an RTC backup register |
| `acc_x`, `acc_y`, `acc_z` | `int16_t` each | ADXL345 axis readings |
| `sensor_state` | `uint8_t` | 0 for normal, 2 for an activity alert |
| `battery_lvl` | `uint8_t` | Estimated battery level from 0 to 100% |

The receiver must use the same field order, sizes and integer interpretation.

## Duty-cycle control

`SubGHz_Phy/App/duty_cycle.c` implements duty cycle protection mechanism. It estimates packet airtime from the configured LoRa parameters, stores the credit balance and time marker in RTC backup registers, and recalculates credit over time. If a packet does not fit the available balance, the application sleeps and retries later.

The main radio parameters are in `SubGHz_Phy/App/subghz_phy_app.h`: frequency, transmit power, spreading factor, coding rate and preamble length. The node ID and report/cooldown intervals are in `SubGHz_Phy/App/subghz_phy_app.c`.

## Project layout

- `Core/Src/main.c` — startup, wake-reason detection and peripheral initialization
- `Core/Src/adxl345.c` — accelerometer configuration and data reads
- `Core/Src/power_monitor.c` — PVD setup and low-supply status
- `SubGHz_Phy/App/subghz_phy_app.c` — radio setup, packet preparation, transmission and shutdown scheduling
- `SubGHz_Phy/App/duty_cycle.c` — airtime estimation and duty-cycle accounting
- `accel_node.ioc` — STM32CubeMX peripheral and pin configuration

## Build and flash

1. Open STM32CubeIDE and choose **File → Import → Existing Projects into Workspace**.
2. Select this `accel_node` directory. It contains the `accel_node.ioc` configuration.
3. Build the project, connect an ST-Link debugger/programmer, then use **Run** or **Debug** to flash the target.
