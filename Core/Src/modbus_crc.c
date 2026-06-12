#include "modbus_crc.h"

uint16_t Modbus_CRC16(const uint8_t *nData, uint16_t wLength)
{
    uint16_t wCrc = 0xFFFF;
    for (uint16_t i = 0; i < wLength; i++) {
        wCrc ^= nData[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (wCrc & 0x0001) {
                wCrc >>= 1;
                wCrc ^= 0xA001;
            } else {
                wCrc >>= 1;
            }
        }
    }
    return wCrc;
}
