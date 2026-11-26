# STM32WLE5JC Firmware Projects

This directory contains independently buildable STM32CubeIDE projects for the STM32WLE5JCIx platform. The four sensor nodes share a LoRa point-to-point packet format and low-power operation, with a different sensor combination in each project.

| Project | Sensors | README |
| --- | --- | --- |
| `accel_node` | ADXL345 accelerometer | [README](accel_node/README.md) |
| `reed_accel_node` | MC-38 normally-open reed switch and ADXL345 | [README](reed_accel_node/README.md) |
| `hall_accel_node` | A3114 Hall sensor and ADXL345 | [README](hall_accel_node/README.md) |
| `ir_accel_node` | TCRT5000 IR sensor with LM393 comparator and ADXL345 | [README](ir_accel_node/README.md) |

Each sensor-node folder contains its own CubeIDE project files and `.ioc` configuration. Import a project folder separately using **File → Import → Existing Projects into Workspace** in STM32CubeIDE.

The additional `base_node` folder is a standalone LoRa test project that transmits `LORA_TEST_HELLO`; it is separate from the four sensor nodes.
