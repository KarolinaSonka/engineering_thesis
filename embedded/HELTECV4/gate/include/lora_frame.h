#ifndef LORA_FRAME_H
#define LORA_FRAME_H

#include <stdint.h>

#pragma pack(push, 1) 
typedef struct {
    uint32_t node_id;      // unique node ID (4 bytes)
    uint8_t  node_type;    // node type: 0 - accel only, 1 - reed + accel, 2 - hall + accel, 3 - ir + accel (1 byte)
    uint16_t msg_counter;  // counter of sent messages (2 bytes)
    int16_t  acc_x;        // X axis <-32768;32767> (2 bytes)
    int16_t  acc_y;        // Y axis (2 bytes)
    int16_t  acc_z;        // Z axis (2 bytes)
    uint8_t  sensor_state; // state of digital sensor - ignored if node_type=0 (1 byte)
    uint8_t  battery_lvl;  // battery level (0%-100%) (1 byte)
} LoRaNodeData;
#pragma pack(pop)

#endif
